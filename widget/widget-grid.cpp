#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <string>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"
#include "misc.hpp"

// color palettes: value [0..1] -> RGBA
// alpha carries intensity, color is always full brightness
static uint32_t palette_flame(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	int r = 255;
	int g = (int)(255 * fmin(1.0, t * 2.0));
	int b = (int)(255 * fmin(1.0, fmax(0.0, t * 2.0 - 1.0)));
	return (a << 24) | (b << 16) | (g << 8) | r;
}

static uint32_t palette_gray(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	return (a << 24) | (255 << 16) | (255 << 8) | 255;
}

enum class DataSource {
	Off,
	PsiSq,     // |ψ|²
	PsiRe,     // Re(ψ)
	PsiIm,     // Im(ψ)
	PsiPhase,  // arg(ψ)
	Potential,  // Re(V)
	COUNT
};

static const char *datasource_names[] = {
	"off", "|psi|^2", "Re(psi)", "Im(psi)", "phase(psi)", "potential"
};

enum class Palette {
	Flame,
	Gray,
	Rainbow,
	Zebra,
	COUNT
};

static const char *palette_names[] = {
	"flame", "gray", "rainbow", "zebra"
};

struct Overlay {
	DataSource source{DataSource::PsiSq};
	Palette palette{Palette::Flame};
	float opacity{1.0f};
	float gamma{0.5f};  // alpha exponent: 1.0=linear, 0.5=sqrt, 0.3=aggressive
	SDL_Texture *tex{};
	int tex_w{};
	int tex_h{};
};

class WidgetGrid : public Widget {
public:
	WidgetGrid(Widget::Info &info) : Widget(info) {
		// default: overlay 0 = potential in gray, overlay 1 = |ψ|² in flame
		m_overlays[0].source = DataSource::Potential;
		m_overlays[0].palette = Palette::Gray;
		m_overlays[0].opacity = 0.6f;
		m_overlays[1].source = DataSource::PsiSq;
		m_overlays[1].palette = Palette::Flame;
		m_overlays[1].opacity = 1.0f;
		m_overlays[2].source = DataSource::Off;
	}

	~WidgetGrid() {
		for(auto &ov : m_overlays)
			if(ov.tex) SDL_DestroyTexture(ov.tex);
	}

	void do_save(ConfigWriter &cfg) override {
		for(int i = 0; i < N_OVERLAYS; i++) {
			cfg.push(i);
			cfg.write("source", (int)m_overlays[i].source);
			cfg.write("palette", (int)m_overlays[i].palette);
			cfg.write("opacity", m_overlays[i].opacity);
			cfg.write("gamma", m_overlays[i].gamma);
			cfg.pop();
		}
		cfg.write("zoom", m_zoom);
		cfg.write("pan_x", m_pan_x);
		cfg.write("pan_y", m_pan_y);
	}

	void do_load(ConfigReader::Node *node) override {
		if(!node) return;
		for(int i = 0; i < N_OVERLAYS; i++) {
			auto *n = node->find(std::to_string(i).c_str());
			if(!n) continue;
			int src = (int)m_overlays[i].source;
			int pal = (int)m_overlays[i].palette;
			n->read("source", src);
			n->read("palette", pal);
			n->read("opacity", m_overlays[i].opacity);
			n->read("gamma", m_overlays[i].gamma);
			m_overlays[i].source = (DataSource)src;
			m_overlays[i].palette = (Palette)pal;
		}
		node->read("zoom", m_zoom);
		node->read("pan_x", m_pan_x);
		node->read("pan_y", m_pan_y);
	}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
		SDL_SetRenderDrawColor(rend, 10, 10, 15, 255);
		SDL_RenderFillRect(rend, nullptr);

		// controls at the top
		draw_controls();
		float ctrl_h = ImGui::GetCursorPosY();

		if(exp.simulations.empty()) {
			ImGui::Text("No simulation");
			return;
		}

		auto &sim = *exp.simulations[0];
		auto &grid = sim.grid;

		// determine texture dimensions from grid
		// axis 0 = x = horizontal, axis 1 = y = vertical
		int tw = (grid.rank >= 1) ? grid.axes[0].points : 1;
		int th = (grid.rank >= 2) ? grid.axes[1].points : 1;

		// available area below controls
		float avail_x = (float)r.x;
		float avail_y = (float)r.y + ctrl_h;
		float avail_w = (float)r.w;
		float avail_h = (float)r.h - ctrl_h;

		// handle pan/zoom
		handle_mouse(r, ctrl_h);

		// compute destination rect with zoom and pan
		// 1D: stretch to fill height. 2D+: maintain 1:1 aspect
		float disp_w, disp_h;
		if(th == 1) {
			// 1D: fill available area, zoom only affects x
			disp_w = avail_w * m_zoom;
			disp_h = avail_h;
		} else {
			float tex_aspect = (float)tw / (float)th;
			float base_scale;
			if(avail_w / avail_h > tex_aspect)
				base_scale = avail_h / th;
			else
				base_scale = avail_w / tw;
			float scale = base_scale * m_zoom;
			disp_w = tw * scale;
			disp_h = th * scale;
		}
		float cx = avail_x + avail_w * 0.5f + m_pan_x;
		float cy = avail_y + avail_h * 0.5f + m_pan_y;

		m_grid_w = tw;
		m_grid_h = th;
		m_dst = {
			cx - disp_w * 0.5f,
			cy - disp_h * 0.5f,
			disp_w,
			disp_h,
		};
		SDL_FRect &dst = m_dst;

		for(int oi = 0; oi < N_OVERLAYS; oi++) {
			auto &ov = m_overlays[oi];
			if(ov.source == DataSource::Off) continue;
			ensure_texture(rend, ov, tw, th);
			fill_texture(ov, sim, tw, th);

			SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
			SDL_SetTextureBlendMode(ov.tex, SDL_BLENDMODE_BLEND);
			SDL_RenderTexture(rend, ov.tex, nullptr, &dst);
		}

		// draw absorbing boundary zones
		if(sim.absorbing_boundary) {
			float w = (float)sim.absorb_width;
			SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
			SDL_SetRenderDrawColor(rend, 200, 40, 40, 40);
			// left
			SDL_FRect ab_left = { dst.x, dst.y, dst.w * w, dst.h };
			SDL_RenderFillRect(rend, &ab_left);
			// right
			SDL_FRect ab_right = { dst.x + dst.w * (1.0f - w), dst.y, dst.w * w, dst.h };
			SDL_RenderFillRect(rend, &ab_right);
			if(grid.rank >= 2) {
				// bottom
				SDL_FRect ab_bot = { dst.x, dst.y + dst.h * (1.0f - w), dst.w, dst.h * w };
				SDL_RenderFillRect(rend, &ab_bot);
				// top
				SDL_FRect ab_top = { dst.x, dst.y, dst.w, dst.h * w };
				SDL_RenderFillRect(rend, &ab_top);
			}
		}

		// draw border around grid
		SDL_SetRenderDrawColor(rend, 80, 80, 80, 255);
		SDL_RenderRect(rend, &dst);

		// LMB click in grid sets cursor position
		if(grid.rank >= 2 && ImGui::IsWindowFocused()) {
			ImVec2 mp = ImGui::GetMousePos();
			int gx, gy;
			if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && screen_to_grid(mp.x, mp.y, gx, gy)) {
				m_view.cursor[0] = gx;
				m_view.cursor[1] = gy;
			}
		}

		// draw slice crosshairs from view
		if(grid.rank >= 2) {
			SDL_SetRenderDrawColor(rend, 100, 100, 100, 120);
			for(int si = 0; si < m_view.n_slices; si++) {
				auto &sl = m_view.slices[si];
				if(!sl.valid) continue;
				for(int d = 0; d < grid.rank; d++) {
					if(d == sl.axis) continue;
					float sx, sy;
					grid_to_screen(sl.pos[0], sl.pos[1], sx, sy);
					if(d == 0)
						SDL_RenderLine(rend, sx, dst.y, sx, dst.y + dst.h);
					else if(d == 1)
						SDL_RenderLine(rend, dst.x, sy, dst.x + dst.w, sy);
				}
			}
		}

		// A: reset view to defaults
		if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A)) {
			m_zoom = 1.0f;
			m_pan_x = 0;
			m_pan_y = 0;
		}
	}

private:
	static constexpr int N_OVERLAYS = 3;
	Overlay m_overlays[N_OVERLAYS]{};
	float m_zoom{1.0f};
	float m_pan_x{0}, m_pan_y{0};
	bool m_dragging{false};
	float m_drag_x{}, m_drag_y{};

	// current display rect (set each frame by do_draw)
	SDL_FRect m_dst{};
	int m_grid_w{1}, m_grid_h{1};

	// screen pixel → grid cell
	bool screen_to_grid(float sx, float sy, int &gx, int &gy) const {
		if(sx < m_dst.x || sx >= m_dst.x + m_dst.w) return false;
		if(sy < m_dst.y || sy >= m_dst.y + m_dst.h) return false;
		gx = (int)((sx - m_dst.x) / m_dst.w * m_grid_w);
		gy = (int)((1.0f - (sy - m_dst.y) / m_dst.h) * m_grid_h);
		if(gx < 0) gx = 0;
		if(gy < 0) gy = 0;
		if(gx >= m_grid_w) gx = m_grid_w - 1;
		if(gy >= m_grid_h) gy = m_grid_h - 1;
		return true;
	}

	// grid cell → screen pixel (center of cell)
	void grid_to_screen(int gx, int gy, float &sx, float &sy) const {
		sx = m_dst.x + ((float)gx + 0.5f) / m_grid_w * m_dst.w;
		sy = m_dst.y + (1.0f - ((float)gy + 0.5f) / m_grid_h) * m_dst.h;
	}

	void handle_mouse(SDL_Rect &r, float ctrl_h) {
		ImVec2 mp = ImGui::GetMousePos();
		bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
		               mp.y >= r.y + ctrl_h && mp.y < r.y + r.h;
		// MMB drag to pan
		if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			m_dragging = true;
			m_drag_x = mp.x;
			m_drag_y = mp.y;
		}
		if(m_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
			m_pan_x += mp.x - m_drag_x;
			m_pan_y += mp.y - m_drag_y;
			m_drag_x = mp.x;
			m_drag_y = mp.y;
		}
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
			m_dragging = false;

		// scroll wheel to zoom
		if(in_rect) {
			float wheel = ImGui::GetIO().MouseWheel;
			if(wheel != 0) {
				float old_zoom = m_zoom;
				m_zoom *= (1.0f + wheel * 0.1f);
				if(m_zoom < 0.1f) m_zoom = 0.1f;
				if(m_zoom > 50.0f) m_zoom = 50.0f;
				// zoom toward mouse position
				float zf = m_zoom / old_zoom;
				float avail_cx = r.x + r.w * 0.5f;
				float avail_cy = r.y + ctrl_h + (r.h - ctrl_h) * 0.5f;
				m_pan_x = (m_pan_x + avail_cx - mp.x) * zf + mp.x - avail_cx;
				m_pan_y = (m_pan_y + avail_cy - mp.y) * zf + mp.y - avail_cy;
			}
		}
	}

	void ensure_texture(SDL_Renderer *rend, Overlay &ov, int w, int h) {
		if(ov.tex && ov.tex_w == w && ov.tex_h == h) return;
		if(ov.tex) SDL_DestroyTexture(ov.tex);
		ov.tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING, w, h);
		ov.tex_w = w;
		ov.tex_h = h;
	}

	void fill_texture(Overlay &ov, Simulation &sim, int tw, int th) {
		void *pixels;
		int pitch;
		if(!SDL_LockTexture(ov.tex, nullptr, &pixels, &pitch)) return;

		auto *psi = sim.psi_front();
		auto *pot = sim.potential;
		size_t total = sim.grid.total_points();

		// find data range for normalization
		double vmin = 0, vmax = 1e-30, amp_max = 1e-30;
		for(size_t i = 0; i < total; i++) {
			double v = sample_value(ov.source, psi[i], pot[i]);
			if(v < vmin) vmin = v;
			if(v > vmax) vmax = v;
			double a = std::abs(psi[i]);
			if(a > amp_max) amp_max = a;
		}
		double range = vmax - vmin;
		if(range < 1e-30) range = 1.0;

		for(int y = 0; y < th; y++) {
			uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
			for(int x = 0; x < tw; x++) {
				// axis 0 = x (column), axis 1 = y (row)
			// grid linear index: coords[0]*stride[0] + coords[1]*stride[1]
			// stride[0] = axes[1].points = th, stride[1] = 1
			size_t idx = (size_t)x * th + (th - 1 - y);
				if(idx >= total) idx = 0;
				double v = sample_value(ov.source, psi[idx], pot[idx]);
				double norm = (v - vmin) / range;

				double amp = std::abs(psi[idx]) / amp_max;
				int alpha = (int)(255 * pow(fmin(1.0, amp), ov.gamma));

				switch(ov.palette) {
					case Palette::Flame: row[x] = palette_flame(norm, ov.gamma); break;
					case Palette::Gray:  row[x] = palette_gray(norm, ov.gamma); break;
					case Palette::Rainbow: {
						uint8_t cr, cg, cb;
						hsv_to_rgb(fmod(norm, 1.0), 1.0, 1.0, cr, cg, cb);
						row[x] = (alpha << 24) | (cb << 16) | (cg << 8) | cr;
					} break;
					case Palette::Zebra: {
						uint8_t c = (uint8_t)(115 + 115 * sin(norm * 8 * M_PI));
						row[x] = (alpha << 24) | (c << 16) | (c << 8) | c;
					} break;
					default: row[x] = 0; break;
				}
			}
		}

		SDL_UnlockTexture(ov.tex);
	}

	double sample_value(DataSource src, std::complex<double> psi, std::complex<double> pot) {
		switch(src) {
			case DataSource::PsiSq:    return std::norm(psi);
			case DataSource::PsiRe:    return psi.real();
			case DataSource::PsiIm:    return psi.imag();
			case DataSource::PsiPhase: return std::arg(psi);
			case DataSource::Potential: return pot.real();
			default: return 0;
		}
	}

	void draw_controls() {
		for(int i = 0; i < N_OVERLAYS; i++) {
			auto &ov = m_overlays[i];
			ImGui::PushID(i);
			ImGui::Text("Overlay %d:", i);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			int src = (int)ov.source;
			if(ImGui::Combo("##src", &src, datasource_names, (int)DataSource::COUNT))
				ov.source = (DataSource)src;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			int pal = (int)ov.palette;
			if(ImGui::Combo("##pal", &pal, palette_names, (int)Palette::COUNT))
				ov.palette = (Palette)pal;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::SliderFloat("##alpha", &ov.opacity, 0.0f, 1.0f, "a%.1f");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			ImGui::SliderFloat("##gamma", &ov.gamma, 0.1f, 2.0f, "g%.1f");
			ImGui::PopID();
		}
	}
};

REGISTER_WIDGET(WidgetGrid,
	.name = "grid",
	.description = "Grid heatmap viewer",
	.hotkey = ImGuiKey_F3,
)
