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
	Spatial,
	COUNT
};

static const char *palette_names[] = {
	"flame", "gray", "rainbow", "zebra", "spatial"
};

// map 2D grid position to hue [0,1] using angle from center
static double spatial_hue(int x, int y, int tw, int th) {
	double cx = (double)x / (tw - 1) - 0.5;
	double cy = (double)y / (th - 1) - 0.5;
	return fmod(atan2(cy, cx) / (2.0 * M_PI) + 1.0, 1.0);
}

struct Overlay {
	DataSource source{DataSource::PsiSq};
	Palette palette{Palette::Flame};
	float opacity{1.0f};
	float gamma{2.0f};  // alpha exponent: 1.0=linear, 0.5=sqrt, 0.3=aggressive
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
		cfg.write("axis_x", m_axis_x);
		cfg.write("axis_y", m_axis_y);
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
		node->read("axis_x", m_axis_x);
		node->read("axis_y", m_axis_y);
	}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override;


private:
	static constexpr int N_OVERLAYS = 3;
	Overlay m_overlays[N_OVERLAYS]{};
	int m_axis_x{0}, m_axis_y{1};  // which axes to display
	float m_zoom{1.0f};
	float m_pan_x{0}, m_pan_y{0};
	bool m_dragging{false};
	float m_drag_x{}, m_drag_y{};

	// current display rect (set each frame by do_draw)
	SDL_FRect m_dst{};
	int m_grid_w{1}, m_grid_h{1};
};


// --- helper functions ---

// screen pixel → grid cell
static bool screen_to_grid(const SDL_FRect &dst, int grid_w, int grid_h,
                            float sx, float sy, int &gx, int &gy)
{
	if(sx < dst.x || sx >= dst.x + dst.w) return false;
	if(sy < dst.y || sy >= dst.y + dst.h) return false;
	gx = (int)((sx - dst.x) / dst.w * grid_w);
	gy = (int)((1.0f - (sy - dst.y) / dst.h) * grid_h);
	if(gx < 0) gx = 0;
	if(gy < 0) gy = 0;
	if(gx >= grid_w) gx = grid_w - 1;
	if(gy >= grid_h) gy = grid_h - 1;
	return true;
}

// grid cell → screen pixel (center of cell)
static void grid_to_screen(const SDL_FRect &dst, int grid_w, int grid_h,
                            int gx, int gy, float &sx, float &sy)
{
	sx = dst.x + ((float)gx + 0.5f) / grid_w * dst.w;
	sy = dst.y + (1.0f - ((float)gy + 0.5f) / grid_h) * dst.h;
}

static void handle_mouse(SDL_Rect &r, float ctrl_h, float &zoom, float &pan_x, float &pan_y,
                         bool &dragging, float &drag_x, float &drag_y)
{
	ImVec2 mp = ImGui::GetMousePos();
	bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
	               mp.y >= r.y + ctrl_h && mp.y < r.y + r.h;
	// MMB drag to pan
	if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		dragging = true;
		drag_x = mp.x;
		drag_y = mp.y;
	}
	if(dragging && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		pan_x += mp.x - drag_x;
		pan_y += mp.y - drag_y;
		drag_x = mp.x;
		drag_y = mp.y;
	}
	if(ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
		dragging = false;

	// scroll wheel to zoom
	if(in_rect) {
		float wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0) {
			float old_zoom = zoom;
			zoom *= (1.0f + wheel * 0.1f);
			if(zoom < 0.1f) zoom = 0.1f;
			if(zoom > 50.0f) zoom = 50.0f;
			// zoom toward mouse position
			float zf = zoom / old_zoom;
			float avail_cx = r.x + r.w * 0.5f;
			float avail_cy = r.y + ctrl_h + (r.h - ctrl_h) * 0.5f;
			pan_x = (pan_x + avail_cx - mp.x) * zf + mp.x - avail_cx;
			pan_y = (pan_y + avail_cy - mp.y) * zf + mp.y - avail_cy;
		}
	}
}

static void ensure_texture(SDL_Renderer *rend, Overlay &ov, int w, int h)
{
	if(ov.tex && ov.tex_w == w && ov.tex_h == h) return;
	if(ov.tex) SDL_DestroyTexture(ov.tex);
	ov.tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	ov.tex_w = w;
	ov.tex_h = h;
}

static double sample_value(DataSource src, std::complex<double> psi, std::complex<double> pot)
{
	switch(src) {
		case DataSource::PsiSq:    return std::norm(psi);
		case DataSource::PsiRe:    return psi.real();
		case DataSource::PsiIm:    return psi.imag();
		case DataSource::PsiPhase: return std::arg(psi);
		case DataSource::Potential: return pot.real();
		default: return 0;
	}
}

static void fill_texture(Overlay &ov, Simulation &sim, int tw, int th,
                          const int *cursor, int axis_x, int axis_y)
{
	void *pixels;
	int pitch;
	if(!SDL_LockTexture(ov.tex, nullptr, &pixels, &pitch)) return;

	auto *psi = sim.psi_front();
	auto *pot = sim.potential;

	auto psi_slice = sim.grid.slice_view(axis_x, axis_y, cursor, psi);
	auto pot_slice = sim.grid.slice_view(axis_x, axis_y, cursor, pot);

	// find data range for normalization (scan displayed slice only)
	double vmin = 0, vmax = 1e-30, amp_max = 1e-30;
	for(int y = 0; y < th; y++) {
		for(int x = 0; x < tw; x++) {
			auto &psi_val = psi_slice.at(x, th - 1 - y);
			auto &pot_val = pot_slice.at(x, th - 1 - y);
			double v = sample_value(ov.source, psi_val, pot_val);
			if(v < vmin) vmin = v;
			if(v > vmax) vmax = v;
			double a = std::abs(psi_val);
			if(a > amp_max) amp_max = a;
		}
	}
	double range = vmax - vmin;
	if(range < 1e-30) range = 1.0;

	for(int y = 0; y < th; y++) {
		uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
		for(int x = 0; x < tw; x++) {
			auto &psi_val = psi_slice.at(x, th - 1 - y);
			auto &pot_val = pot_slice.at(x, th - 1 - y);
			double v = sample_value(ov.source, psi_val, pot_val);
			double norm = (v - vmin) / range;

			double amp = std::abs(psi_val) / amp_max;
			int alpha = (int)(255 * pow(fmin(1.0, amp), 1.0 / ov.gamma));

			switch(ov.palette) {
				case Palette::Flame: row[x] = palette_flame(norm, 1.0 / ov.gamma); break;
				case Palette::Gray:  row[x] = palette_gray(norm, 1.0 / ov.gamma); break;
				case Palette::Rainbow: {
					uint8_t cr, cg, cb;
					hsv_to_rgb(fmod(norm, 1.0), 1.0, 1.0, cr, cg, cb);
					row[x] = (alpha << 24) | (cb << 16) | (cg << 8) | cr;
				} break;
			case Palette::Zebra: {
				uint8_t c = (uint8_t)(115 + 115 * sin(norm * 2 * M_PI));
				row[x] = (alpha << 24) | (c << 16) | (c << 8) | c;
			} break;
			case Palette::Spatial: {
				double hue = spatial_hue(x, th - 1 - y, tw, th);
				uint8_t cr, cg, cb;
				hsv_to_rgb(hue, 0.8, 1.0, cr, cg, cb);
				row[x] = (alpha << 24) | (cb << 16) | (cg << 8) | cr;
			} break;
			default: row[x] = 0; break;
			}
		}
	}

	SDL_UnlockTexture(ov.tex);
}

static void draw_controls(Overlay overlays[], int n_overlays)
{
	for(int i = 0; i < n_overlays; i++) {
		auto &ov = overlays[i];
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
		ImGui::SliderFloat("##gamma", &ov.gamma, 0.5f, 10.0f, "g%.1f");
		ImGui::PopID();
	}
}


void WidgetGrid::do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r)
{
	SDL_SetRenderDrawColor(rend, 10, 10, 15, 255);
	SDL_RenderFillRect(rend, nullptr);

	// controls at the top
	draw_controls(m_overlays, N_OVERLAYS);

	if(!exp.simulations.empty() && exp.simulations[0]->grid.rank > 2) {
		auto &g = exp.simulations[0]->grid;
		ImGui::AxisCombo("##ax_x", &m_axis_x, g);
		ImGui::SameLine();
		ImGui::AxisCombo("##ax_y", &m_axis_y, g);
	}

	float ctrl_h = ImGui::GetCursorPosY();

	if(exp.simulations.empty()) {
		ImGui::Text("No simulation");
		return;
	}

	auto &sim = *exp.simulations[0];
	auto &grid = sim.grid;

	// clamp axis selection
	if(m_axis_x >= grid.rank) m_axis_x = 0;
	if(m_axis_y >= grid.rank) m_axis_y = grid.rank > 1 ? 1 : 0;
	if(m_axis_x == m_axis_y && grid.rank > 1) m_axis_y = (m_axis_x + 1) % grid.rank;

	// determine texture dimensions from selected axes
	int tw = grid.axes[m_axis_x].points;
	int th = (grid.rank >= 2) ? grid.axes[m_axis_y].points : 1;

	// available area below controls
	float avail_x = (float)r.x;
	float avail_y = (float)r.y + ctrl_h;
	float avail_w = (float)r.w;
	float avail_h = (float)r.h - ctrl_h;

	// handle pan/zoom
	handle_mouse(r, ctrl_h, m_zoom, m_pan_x, m_pan_y, m_dragging, m_drag_x, m_drag_y);

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
		fill_texture(ov, sim, tw, th, m_view.cursor, m_axis_x, m_axis_y);

		SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
		SDL_SetTextureBlendMode(ov.tex, SDL_BLENDMODE_BLEND);
		SDL_RenderTexture(rend, ov.tex, nullptr, &dst);
	}

	// draw absorbing boundary zones with cos² gradient
	if(sim.absorbing_boundary) {
		float w = (float)sim.absorb_width;
		int n_strips = (int)(w * tw);
		if(n_strips < 1) n_strips = 1;
		float strip_w = dst.w * w / n_strips;
		SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
		for(int i = 0; i < n_strips; i++) {
			float t_lo = cosf(0.5f * M_PI * (i + 1) / (n_strips));
			float alpha = t_lo * t_lo * 80;
			SDL_SetRenderDrawColor(rend, 25, 25, 200, (uint8_t)alpha);
			// left
			SDL_FRect r_left = { dst.x + i * strip_w, dst.y, strip_w + 1, dst.h };
			SDL_RenderFillRect(rend, &r_left);
			// right
			SDL_FRect r_right = { dst.x + dst.w - (i + 1) * strip_w, dst.y, strip_w + 1, dst.h };
			SDL_RenderFillRect(rend, &r_right);
		}
		if(grid.rank >= 2) {
			float strip_h = dst.h * w / n_strips;
			for(int i = 0; i < n_strips; i++) {
				float t_lo = cosf(0.5f * M_PI * (i + 1) / (n_strips));
				float alpha = t_lo * t_lo * 80;
				SDL_SetRenderDrawColor(rend, 25, 25, 200, (uint8_t)alpha);
				// top (grid y=max is screen top)
				SDL_FRect r_top = { dst.x, dst.y + i * strip_h, dst.w, strip_h + 1 };
				SDL_RenderFillRect(rend, &r_top);
				// bottom
				SDL_FRect r_bot = { dst.x, dst.y + dst.h - (i + 1) * strip_h, dst.w, strip_h + 1 };
				SDL_RenderFillRect(rend, &r_bot);
			}
		}
	}

	// draw border around grid
	SDL_SetRenderDrawColor(rend, 80, 80, 80, 255);
	SDL_RenderRect(rend, &dst);

	// hover in grid updates cursor position on selected axes
	if(grid.rank >= 2 && ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ImVec2 mp = ImGui::GetMousePos();
		int gx, gy;
		if(screen_to_grid(m_dst, m_grid_w, m_grid_h, mp.x, mp.y, gx, gy)) {
			m_view.cursor[m_axis_x] = gx;
			m_view.cursor[m_axis_y] = gy;
		}
	}

	// draw cursor crosshairs
	if(grid.rank >= 2) {
		SDL_SetRenderDrawColor(rend, 230, 50, 50, 50);
		float sx, sy;
		grid_to_screen(m_dst, m_grid_w, m_grid_h, m_view.cursor[m_axis_x], m_view.cursor[m_axis_y], sx, sy);
		SDL_RenderLine(rend, sx, dst.y, sx, dst.y + dst.h);
		SDL_RenderLine(rend, dst.x, sy, dst.x + dst.w, sy);
	}

	// A: reset view to defaults
	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A)) {
		m_zoom = 1.0f;
		m_pan_x = 0;
		m_pan_y = 0;
	}

	// D: dump displayed slice to dump.txt
	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_D)) {
		auto *psi = sim.psi_front();
		auto *pot = sim.potential;

		auto psi_slice = sim.grid.slice_view(m_axis_x, m_axis_y, m_view.cursor, psi);
		auto pot_slice = sim.grid.slice_view(m_axis_x, m_axis_y, m_view.cursor, pot);

		int sx = (tw + 255) / 256;
		int sy_d = (th + 127) / 128;
		int ox = tw / sx;
		int oy = th / sy_d;

		double max_val = 1e-30;
		for(int iy = 0; iy < th; iy++)
			for(int ix = 0; ix < tw; ix++) {
				double v = std::norm(psi_slice.at(ix, iy));
				if(v > max_val) max_val = v;
			}

		FILE *f = fopen("dump.txt", "w");
		const char *lx = grid.axes[m_axis_x].label[0] ? grid.axes[m_axis_x].label : "?";
		const char *ly = grid.axes[m_axis_y].label[0] ? grid.axes[m_axis_y].label : "?";
		fprintf(f, "# t=%.4e  axes=%s/%s  %dx%d  max=%.4e\n", sim.time(), lx, ly, ox, oy, max_val);
		for(int d = 0; d < grid.rank; d++) {
			if(d == m_axis_x || d == m_axis_y) continue;
			const char *l = grid.axes[d].label[0] ? grid.axes[d].label : "?";
			fprintf(f, "# %s=%d\n", l, m_view.cursor[d]);
		}
		fprintf(f, "# |psi|^2 (potential shown as ||):\n");
		const char *shades = "_.:-=o+*#%@";
		int nshades = 11;
		for(int iy = oy - 1; iy >= 0; iy--) {
			for(int ix = 0; ix < ox; ix++) {
				int gx = ix * sx + sx/2;
				int gy = iy * sy_d + sy_d/2;
				if(pot_slice.at(gx, gy).real() > 0)
					fputc('|', f);
				else {
					double v = pow(std::norm(psi_slice.at(gx, gy)) / max_val, 0.15);
					int si = (int)(v * (nshades - 1));
					if(si >= nshades) si = nshades - 1;
					if(si < 0) si = 0;
					fputc(shades[si], f);
				}
			}
			fputc('\n', f);
		}
		fclose(f);
		fprintf(stderr, "dumped %s/%s to dump.txt (%dx%d)\n", lx, ly, ox, oy);
	}
}


REGISTER_WIDGET(WidgetGrid,
	.name = "grid",
	.description = "Grid heatmap viewer",
	.hotkey = ImGuiKey_F3,
)
