#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <string>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"
#include "config.hpp"
#include "misc.hpp"
#include "colors.hpp"
#include "datasource.hpp"

struct Overlay {
	DataSource source{DataSource::PsiSq};
	Palette palette{Palette::Flame};
	float opacity{1.0f};
	float gamma{2.0f};
	SDL_Texture *tex{};
	int tex_w{};
	int tex_h{};
};

class WidgetTrace : public Widget {
public:
	WidgetTrace(Widget::Info &info) : Widget(info) {
		m_overlays[0].source = DataSource::Potential;
		m_overlays[0].palette = Palette::Gray;
		m_overlays[0].opacity = 0.6f;
		m_overlays[1].source = DataSource::PsiSq;
		m_overlays[1].palette = Palette::Flame;
		m_overlays[1].opacity = 1.0f;
		m_overlays[2].source = DataSource::Off;
	}

	~WidgetTrace() {
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
		cfg.write("axis", m_axis);
		cfg.write("marginal", m_marginal ? 1 : 0);
		cfg.write("step_interval", m_step_interval);
		cfg.write("history", m_history_depth);
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
		node->read("axis", m_axis);
		int marg = 0; node->read("marginal", marg); m_marginal = marg;
		node->read("step_interval", m_step_interval);
		node->read("history", m_history_depth);
	}

	void do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r) override;

private:
	void snapshot(const ExtractionResult &res);
	void update_overlays(SDL_Renderer *rend, int tw, int th, bool horiz);
	void draw_controls(SimContext &ctx);
	void draw_cursor(SDL_Renderer *rend, bool horiz);

	static constexpr int N_OVERLAYS = 3;
	Overlay m_overlays[N_OVERLAYS]{};
	int m_axis{0};
	bool m_marginal{false};
	int m_step_interval{1};
	int m_history_depth{300};

	Camera3D m_camera;

	// ring buffer: [history_depth × axis_points]
	std::vector<psi_t> m_psi_history;
	std::vector<psi_t> m_pot_history;
	int m_history_head{0};
	int m_history_count{0};
	int m_axis_points{0};

	size_t m_last_step{0};

	SDL_FRect m_dst{};
};


// --- helper functions ---

static void ensure_texture(SDL_Renderer *rend, Overlay &ov, int w, int h)
{
	if(ov.tex && ov.tex_w == w && ov.tex_h == h) return;
	if(ov.tex) SDL_DestroyTexture(ov.tex);
	ov.tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	SDL_SetTextureBlendMode(ov.tex, SDL_BLENDMODE_BLEND);
	ov.tex_w = w;
	ov.tex_h = h;
}

static void render_texture(Overlay &ov, const psi_t *psi_buf, const psi_t *pot_buf,
                            int tw, int th)
{
	void *pixels;
	int pitch;
	if(!SDL_LockTexture(ov.tex, nullptr, &pixels, &pitch)) return;

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

static void draw_overlay_controls(Overlay overlays[], int n_overlays)
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


// --- WidgetTrace implementation ---

void WidgetTrace::snapshot(const ExtractionResult &res)
{
	int axis_pts = res.shape[0];
	if(axis_pts <= 0) return;

	if(axis_pts != m_axis_points) {
		m_axis_points = axis_pts;
		m_psi_history.clear();
		m_pot_history.clear();
		m_psi_history.resize(m_history_depth * axis_pts, psi_t(0, 0));
		m_pot_history.resize(m_history_depth * axis_pts, psi_t(0, 0));
		m_history_head = 0;
		m_history_count = 0;
	}

	psi_t *psi_dest = &m_psi_history[m_history_head * axis_pts];
	psi_t *pot_dest = &m_pot_history[m_history_head * axis_pts];
	int n = std::min(axis_pts, (int)res.psi.size());
	std::copy(res.psi.begin(), res.psi.begin() + n, psi_dest);
	int np = std::min(axis_pts, (int)res.pot.size());
	std::copy(res.pot.begin(), res.pot.begin() + np, pot_dest);

	m_history_head = (m_history_head + 1) % m_history_depth;
	if(m_history_count < m_history_depth) m_history_count++;
}

void WidgetTrace::update_overlays(SDL_Renderer *rend, int tw, int th, bool horiz)
{
	if(m_history_count == 0 || m_axis_points == 0) return;

	for(int oi = 0; oi < N_OVERLAYS; oi++) {
		auto &ov = m_overlays[oi];
		if(ov.source == DataSource::Off) continue;
		ensure_texture(rend, ov, tw, th);

		std::vector<psi_t> psi_buf(tw * th, psi_t(0, 0));
		std::vector<psi_t> pot_buf(tw * th, psi_t(0, 0));

		int samples = m_history_count;
		int time_slots = horiz ? th : tw;
		int offset = time_slots - samples;

		for(int t = 0; t < samples; t++) {
			int ring_idx = (m_history_head - samples + t + m_history_depth) % m_history_depth;
			psi_t *psi_src = &m_psi_history[ring_idx * m_axis_points];
			psi_t *pot_src = &m_pot_history[ring_idx * m_axis_points];
			int ti = t + offset;

			if(horiz) {
				for(int s = 0; s < m_axis_points; s++) {
					psi_buf[s * th + ti] = psi_src[s];
					pot_buf[s * th + ti] = pot_src[s];
				}
			} else {
				for(int s = 0; s < m_axis_points; s++) {
					psi_buf[ti * th + s] = psi_src[s];
					pot_buf[ti * th + s] = pot_src[s];
				}
			}
		}

		render_texture(ov, psi_buf.data(), pot_buf.data(), tw, th);
	}
}

void WidgetTrace::draw_controls(SimContext &ctx)
{
	auto &gm = ctx.state().grid;

	ImGui::ToggleButton("L", &m_view.lock);
	ImGui::SameLine();

	// axis combo — inline since we have GridMeta not Grid
	if(gm.rank > 1) {
		ImGui::SetNextItemWidth(80);
		if(ImGui::BeginCombo("##axis", gm.axes[m_axis].label)) {
			for(int d = 0; d < gm.rank; d++) {
				if(ImGui::Selectable(gm.axes[d].label, d == m_axis))
					m_axis = d;
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
	}

	if(ImGui::Button(m_marginal ? "Marginal" : "Slice"))
		m_marginal = !m_marginal;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::SliderInt("##step", &m_step_interval, 1, 8, "/%d");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	if(ImGui::SliderInt("##history", &m_history_depth, 100, 1000, "n %d")) {
		int old_depth = (int)(m_psi_history.size() / std::max(1, m_axis_points));
		if(m_history_depth != old_depth) {
			m_psi_history.clear();
			m_pot_history.clear();
			m_psi_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
			m_pot_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
			m_history_head = 0;
			m_history_count = 0;
		}
	}

	// Measure button + M key (right-aligned)
	int mact = ImGui::MeasureButton();
	if(mact == 1) ctx.push(CmdMeasure{m_axis});
	if(mact == 2) ctx.push(CmdDecohere{m_axis});

	if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		draw_overlay_controls(m_overlays, N_OVERLAYS);
}

void WidgetTrace::draw_cursor(SDL_Renderer *rend, bool horiz)
{
	if(m_axis_points < 1) return;

	if(ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ImVec2 mp = ImGui::GetMousePos();
		if(mp.x >= m_dst.x && mp.x < m_dst.x + m_dst.w &&
		   mp.y >= m_dst.y && mp.y < m_dst.y + m_dst.h) {
			if(horiz) {
				float frac = (mp.x - m_dst.x) / m_dst.w;
				m_view.cursor[m_axis] = (int)(frac * m_axis_points);
			} else {
				float frac = 1.0f - (mp.y - m_dst.y) / m_dst.h;
				m_view.cursor[m_axis] = (int)(frac * m_axis_points);
			}
			if(m_view.cursor[m_axis] < 0) m_view.cursor[m_axis] = 0;
			if(m_view.cursor[m_axis] >= m_axis_points) m_view.cursor[m_axis] = m_axis_points - 1;
		}
	}

	SDL_SetRenderDrawColor(rend, colors::cursor_cross.r, colors::cursor_cross.g,
	                       colors::cursor_cross.b, colors::cursor_cross.a);
	int c = m_view.cursor[m_axis];
	if(horiz) {
		float sx = m_dst.x + ((float)c + 0.5f) / m_axis_points * m_dst.w;
		SDL_RenderLine(rend, sx, m_dst.y, sx, m_dst.y + m_dst.h);
	} else {
		float sy = m_dst.y + (1.0f - ((float)c + 0.5f) / m_axis_points) * m_dst.h;
		SDL_RenderLine(rend, m_dst.x, sy, m_dst.x + m_dst.w, sy);
	}
}

void WidgetTrace::do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	// sync camera from shared view when locked
	if(m_view.lock)
		m_camera = m_view.camera;

	auto &st = ctx.state();
	auto &gm = st.grid;

	if(gm.rank == 0) return;
	if(m_axis >= gm.rank) m_axis = 0;

	// declare extraction request
	bool want_marginal = m_marginal && gm.rank > 1;
	ExtractionRequest req{};
	req.axes[0] = m_axis;
	for(int a = 1; a < MAX_RANK; a++) req.axes[a] = -1;
	for(int d = 0; d < MAX_RANK; d++) req.cursor[d] = m_view.cursor[d];
	req.marginal = want_marginal;
	ctx.request(req);

	// detect reset (step_count went backwards)
	if(st.step_count < m_last_step) {
		m_psi_history.assign(m_psi_history.size(), psi_t(0, 0));
		m_pot_history.assign(m_pot_history.size(), psi_t(0, 0));
		m_history_head = 0;
		m_history_count = 0;
		m_last_step = 0;
		for(auto &ov : m_overlays) {
			if(ov.tex) { SDL_DestroyTexture(ov.tex); ov.tex = nullptr; }
		}
	}

	// snapshot from extraction result when step advances
	auto *res = st.find(req);
	if(res && !res->psi.empty() && st.running &&
	   st.step_count >= m_last_step + (size_t)m_step_interval) {
		snapshot(*res);
		m_last_step = st.step_count;
	}

	draw_controls(ctx);
	float ctrl_h = ImGui::GetCursorPosY();

	if(m_axis_points > 0) {
		bool horiz = (gm.cs.dim_of(m_axis) == 0);
		int tw = horiz ? m_axis_points   : m_history_depth;
		int th = horiz ? m_history_depth : m_axis_points;
		update_overlays(rend, tw, th, horiz);

		// pan/zoom via shared camera mechanism
		m_camera.handle_mouse(r);

		float avail_h = r.h - ctrl_h - 10;
		float aspect = (float)tw / th;

		// compute spatial extent from camera — same math as helix
		// helix builds VP with square aspect (w×w), we replicate that
		mat4 vp = m_camera.build(r.w, r.w);
		vec3 pl = vp.transform({-1, 0, 0});
		vec3 pr = vp.transform({ 1, 0, 0});
		float sl = (1.0f + (float)pl.x) * 0.5f;
		float sr = (1.0f + (float)pr.x) * 0.5f;

		SDL_FRect dst;
		if(horiz) {
			dst.x = r.x + sl * r.w;
			dst.w = (sr - sl) * r.w;
			dst.h = dst.w / aspect;
			dst.y = r.y + ctrl_h + avail_h * 0.5f + m_camera.pan_y - dst.h * 0.5f;
		} else {
			dst.h = (sr - sl) * r.w;  // spatial axis is vertical
			dst.w = dst.h * aspect;
			dst.y = r.y + ctrl_h + (1.0f - sr) * r.w;  // flip for vertical
			dst.x = r.x + r.w * 0.5f + m_camera.pan_x - dst.w * 0.5f;
		}

		for(auto &ov : m_overlays) {
			if(ov.source == DataSource::Off || !ov.tex) continue;
			SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
			SDL_RenderTexture(rend, ov.tex, nullptr, &dst);
		}

		SDL_SetRenderDrawColor(rend, colors::grid_border.r, colors::grid_border.g,
		                       colors::grid_border.b, colors::grid_border.a);
		SDL_RenderRect(rend, &dst);

		m_dst = dst;
		draw_cursor(rend, horiz);
	}

	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A)) {
		m_psi_history.clear();
		m_pot_history.clear();
		m_psi_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
		m_pot_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
		m_history_head = 0;
		m_history_count = 0;
		m_camera = Camera3D{};
	}

	// write back camera to shared view when locked
	if(m_view.lock)
		m_view.camera = m_camera;
}


REGISTER_WIDGET(WidgetTrace,
	.name = "trace",
	.description = "Time trace viewer",
	.hotkey = ImGuiKey_F4,
)
