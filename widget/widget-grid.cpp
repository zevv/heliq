#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <string>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"
#include "config.hpp"
#include "misc.hpp"
#include "style.hpp"
#include "log.hpp"

// color palettes: value [0..1] -> RGBA
// alpha carries intensity, color is always full brightness
#include "datasource.hpp"

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
		cfg.write("zoom", m_view_state.zoom);
		cfg.write("pan_x", m_view_state.pan_x);
		cfg.write("pan_y", m_view_state.pan_y);
		cfg.write("axis_x", m_axis_x);
		cfg.write("axis_y", m_axis_y);
		cfg.write("marginal", m_marginal ? 1 : 0);
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
		node->read("zoom", m_view_state.zoom);
		node->read("pan_x", m_view_state.pan_x);
		node->read("pan_y", m_view_state.pan_y);
		node->read("axis_x", m_axis_x);
		node->read("axis_y", m_axis_y);
		int marg = 0; node->read("marginal", marg); m_marginal = marg;
	}

	void do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r) override;

private:
	void update_overlays_from_result(const ExtractionResult &res, SDL_Renderer *rend, int tw, int th);
	void draw_absorb_boundary(SDL_Renderer *rend, const PublishedState &st, int tw);
	void draw_cursor(SDL_Renderer *rend, const Grid &grid);
	void dump_result(const ExtractionResult &res, int tw, int th);
	SDL_FRect compute_display_rect(const Axis &ax, const Axis &ay, float avail_x, float avail_y, float avail_w, float avail_h);

	static constexpr int N_OVERLAYS = 3;
	Overlay m_overlays[N_OVERLAYS]{};
	int m_axis_x{0}, m_axis_y{1};  // which axes to display
	bool m_marginal{false};         // true = sum over hidden axes

	struct {
		float zoom{1.0f};
		float pan_x{0}, pan_y{0};
		bool dragging{false};
		float drag_x{}, drag_y{};
	} m_view_state;

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
	// RMB drag to pan
	if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		dragging = true;
		drag_x = mp.x;
		drag_y = mp.y;
	}
	if(dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		pan_x += mp.x - drag_x;
		pan_y += mp.y - drag_y;
		drag_x = mp.x;
		drag_y = mp.y;
	}
	if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
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


// render a 2D psi/pot buffer into an SDL texture overlay
static void render_texture(Overlay &ov, const psi_t *psi_buf, const psi_t *pot_buf,
                            int tw, int th)
{
	void *pixels;
	int pitch;
	if(!SDL_LockTexture(ov.tex, nullptr, &pixels, &pitch)) return;

	// find data range for normalization
	double vmin = 0, vmax = 1e-30, amp_max = 1e-30;
	for(int i = 0; i < tw * th; i++) {
		double v = sample_value(ov.source, psi_buf[i], pot_buf[i]);
		if(v < vmin) vmin = v;
		if(v > vmax) vmax = v;
		double a = std::abs(psi_buf[i]);
		if(a > amp_max) amp_max = a;
	}
	double range = vmax - vmin;
	if(range < 1e-30) range = 1.0;

	for(int y = 0; y < th; y++) {
		uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
		for(int x = 0; x < tw; x++) {
			auto &psi_val = psi_buf[x * th + (th - 1 - y)];
			auto &pot_val = pot_buf[x * th + (th - 1 - y)];
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


void WidgetGrid::update_overlays_from_result(const ExtractionResult &res,
	SDL_Renderer *rend, int tw, int th)
{
	for(int oi = 0; oi < N_OVERLAYS; oi++) {
		auto &ov = m_overlays[oi];
		if(ov.source == DataSource::Off) continue;
		ensure_texture(rend, ov, tw, th);
		render_texture(ov, res.psi.data(), res.pot.data(), tw, th);
		SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
		SDL_SetTextureBlendMode(ov.tex, SDL_BLENDMODE_BLEND);
		SDL_RenderTexture(rend, ov.tex, nullptr, &m_dst);
	}
}


void WidgetGrid::draw_absorb_boundary(SDL_Renderer *rend, const PublishedState &st, int tw)
{
	if(!st.absorbing_boundary) return;

	float w = (float)st.absorb_width;
	int n_strips = (int)(w * tw);
	if(n_strips < 1) n_strips = 1;
	SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

	auto ac = Style::color(Style::Absorb);
	float strip_w = m_dst.w * w / n_strips;
	for(int i = 0; i < n_strips; i++) {
		float t = cosf(0.5f * M_PI * (i + 1) / n_strips);
		uint8_t alpha = (uint8_t)(t * t * 80);
		SDL_SetRenderDrawColor(rend, (uint8_t)(ac.r*255), (uint8_t)(ac.g*255), (uint8_t)(ac.b*255), alpha);
		SDL_FRect rl = { m_dst.x + i * strip_w, m_dst.y, strip_w + 1, m_dst.h };
		SDL_FRect rr = { m_dst.x + m_dst.w - (i + 1) * strip_w, m_dst.y, strip_w + 1, m_dst.h };
		SDL_RenderFillRect(rend, &rl);
		SDL_RenderFillRect(rend, &rr);
	}
	if(st.grid.rank >= 2) {
		float strip_h = m_dst.h * w / n_strips;
		for(int i = 0; i < n_strips; i++) {
			float t = cosf(0.5f * M_PI * (i + 1) / n_strips);
			uint8_t alpha = (uint8_t)(t * t * 80);
			SDL_SetRenderDrawColor(rend, (uint8_t)(ac.r*255), (uint8_t)(ac.g*255), (uint8_t)(ac.b*255), alpha);
			SDL_FRect rt = { m_dst.x, m_dst.y + i * strip_h, m_dst.w, strip_h + 1 };
			SDL_FRect rb = { m_dst.x, m_dst.y + m_dst.h - (i + 1) * strip_h, m_dst.w, strip_h + 1 };
			SDL_RenderFillRect(rend, &rt);
			SDL_RenderFillRect(rend, &rb);
		}
	}
}


void WidgetGrid::draw_cursor(SDL_Renderer *rend, const Grid &grid)
{
	if(grid.rank < 2) return;

	// update cursor from mouse
	if(ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		ImVec2 mp = ImGui::GetMousePos();
		int gx, gy;
		if(screen_to_grid(m_dst, m_grid_w, m_grid_h, mp.x, mp.y, gx, gy)) {
			m_view.cursor[m_axis_x] = gx;
			m_view.cursor[m_axis_y] = gy;
		}
	}

	// draw crosshairs
	SDL_SetRenderDrawColor(rend, Style::color(Style::CursorCross));
	float sx, sy;
	grid_to_screen(m_dst, m_grid_w, m_grid_h, m_view.cursor[m_axis_x], m_view.cursor[m_axis_y], sx, sy);
	SDL_RenderLine(rend, sx, m_dst.y, sx, m_dst.y + m_dst.h);
	SDL_RenderLine(rend, m_dst.x, sy, m_dst.x + m_dst.w, sy);
}


void WidgetGrid::dump_result(const ExtractionResult &res, int tw, int th)
{
	if(res.psi.empty()) return;

	int sx = (tw + 255) / 256;
	int sy = (th + 127) / 128;
	int ox = tw / sx, oy = th / sy;

	double max_val = 1e-30;
	for(int i = 0; i < tw * th; i++) {
		double v = std::norm(res.psi[i]);
		if(v > max_val) max_val = v;
	}

	FILE *f = fopen("dump.txt", "w");
	fprintf(f, "# %dx%d  max=%.4e\n", ox, oy, max_val);

	const char *shades = "_.:-=o+*#%@";
	int nshades = 11;
	for(int iy = oy - 1; iy >= 0; iy--) {
		for(int ix = 0; ix < ox; ix++) {
			int gx = ix * sx + sx / 2;
			int gy = iy * sy + sy / 2;
			int idx = gx * th + gy;
			if(idx < (int)res.pot.size() && res.pot[idx].real() > 0) {
				fputc('|', f);
			} else {
				double v = pow(std::norm(res.psi[idx]) / max_val, 0.15);
				int si = (int)(v * (nshades - 1));
				si = std::clamp(si, 0, nshades - 1);
				fputc(shades[si], f);
			}
		}
		fputc('\n', f);
	}
	fclose(f);
	linf("dumped to dump.txt (%dx%d)", ox, oy);
}


SDL_FRect WidgetGrid::compute_display_rect(const Axis &ax, const Axis &ay, float avail_x, float avail_y, float avail_w, float avail_h)
{
	float disp_w, disp_h;
	if(ay.points == 1) {
		disp_w = avail_w * m_view_state.zoom;
		disp_h = avail_h;
	} else {
		float phys_aspect = (float)((ax.max - ax.min) / (ay.max - ay.min));
		float base_scale = (avail_w / avail_h > phys_aspect) ? avail_h : avail_w / phys_aspect;
		float scale = base_scale * m_view_state.zoom;
		disp_w = phys_aspect * scale;
		disp_h = scale;
	}
	float cx = avail_x + avail_w * 0.5f + m_view_state.pan_x;
	float cy = avail_y + avail_h * 0.5f + m_view_state.pan_y;
	return { cx - disp_w * 0.5f, cy - disp_h * 0.5f, disp_w, disp_h };
}


void WidgetGrid::do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	auto &st = ctx.state();
	auto &gm = st.grid;
		
	ImGui::SameLine();

	SDL_SetRenderDrawColor(rend, Style::color(Style::BgGrid));
	SDL_RenderFillRect(rend, nullptr);

	if(gm.rank == 0) { ImGui::Text("No simulation"); return; }

	// clamp axis selection
	if(m_axis_x >= gm.rank) m_axis_x = 0;
	if(m_axis_y >= gm.rank) m_axis_y = gm.rank > 1 ? 1 : 0;
	if(m_axis_x == m_axis_y && gm.rank > 1) m_axis_y = (m_axis_x + 1) % gm.rank;

	// axis selection and slice/marginal mode
	if(gm.rank > 2) {
		// AxisCombo needs a Grid — construct a temporary from GridMeta
		Grid grid_tmp{};
		grid_tmp.rank = gm.rank;
		for(int d = 0; d < gm.rank; d++) grid_tmp.axes[d] = gm.axes[d];
		ImGui::AxisCombo("##ax_x", &m_axis_x, grid_tmp);
		ImGui::SameLine();
		ImGui::AxisCombo("##ax_y", &m_axis_y, grid_tmp);
		ImGui::SameLine();
		if(ImGui::Button(m_marginal ? "Marginal" : "Slice"))
			m_marginal = !m_marginal;
	}

	// Measure button + M key (right-aligned)
	int mact = ImGui::MeasureButton();
	if(mact == 1) { ctx.push(CmdMeasure{m_axis_x}); ctx.push(CmdMeasure{m_axis_y}); }
	if(mact == 2) { ctx.push(CmdDecohere{m_axis_x}); ctx.push(CmdDecohere{m_axis_y}); }

	// capture first-row height before optional secondary controls
	float ctrl_h = ImGui::GetCursorPosY();

	if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		draw_controls(m_overlays, N_OVERLAYS);

	int tw = gm.axes[m_axis_x].points;
	int th = (gm.rank >= 2) ? gm.axes[m_axis_y].points : 1;

	handle_mouse(r, ctrl_h, m_view_state.zoom, m_view_state.pan_x, m_view_state.pan_y, m_view_state.dragging, m_view_state.drag_x, m_view_state.drag_y);

	m_grid_w = tw;
	m_grid_h = th;
	m_dst = compute_display_rect(gm.axes[m_axis_x], gm.axes[m_axis_y], r.x, r.y + ctrl_h, r.w, r.h - ctrl_h);

	// declare extraction request for next frame
	bool want_marginal = m_marginal && gm.rank > 2;
	ExtractionRequest req{};
	req.axes[0] = m_axis_x;
	req.axes[1] = (gm.rank >= 2) ? m_axis_y : -1;
	for(int a = 2; a < MAX_RANK; a++) req.axes[a] = -1;
	for(int d = 0; d < MAX_RANK; d++) req.cursor[d] = m_view.cursor[d];
	req.marginal = want_marginal;
	ctx.request(req);

	// read result from previous frame
	auto *res = st.find(req);
	if(res && !res->psi.empty()) {
		update_overlays_from_result(*res, rend, tw, th);
	}

	draw_absorb_boundary(rend, st, tw);

	SDL_SetRenderDrawColor(rend, Style::color(Style::GridBorder));
	SDL_RenderRect(rend, &m_dst);

	Grid grid_tmp{};
	grid_tmp.rank = gm.rank;
	for(int d = 0; d < gm.rank; d++) grid_tmp.axes[d] = gm.axes[d];
	draw_cursor(rend, grid_tmp);

	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A))
		m_view_state = {};

	// D: dump current extraction to file
	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_D) && res)
		dump_result(*res, tw, th);
}


REGISTER_WIDGET(WidgetGrid,
	.name = "grid",
	.description = "Grid heatmap viewer",
	.hotkey = ImGuiKey_F3,
)
