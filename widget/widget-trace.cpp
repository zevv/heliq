#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <string>
#include <chrono>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
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
		cfg.write("interval", m_snapshot_interval);
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
		node->read("interval", m_snapshot_interval);
		node->read("history", m_history_depth);
	}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override;

private:
	void snapshot(Simulation &sim);
	void update_overlays(SDL_Renderer *rend, int tw, int th, bool horiz);
	void draw_controls(const Grid &grid);
	void draw_cursor(SDL_Renderer *rend, bool horiz);

	static constexpr int N_OVERLAYS = 3;
	Overlay m_overlays[N_OVERLAYS]{};
	int m_axis{0};
	bool m_marginal{false};
	float m_snapshot_interval{0.1f};  // seconds
	int m_history_depth{300};

	// ring buffer: [history_depth × axis_points]
	std::vector<psi_t> m_psi_history;
	std::vector<psi_t> m_pot_history;
	int m_history_head{0};  // write index
	int m_history_count{0}; // samples written
	int m_axis_points{0};   // cached from last snapshot

	bool m_started{false};
	std::chrono::steady_clock::time_point m_last_snapshot{};

	// display rect for cursor
	SDL_FRect m_dst{};
};


// --- helper functions (duplicated from grid, extractable later) ---

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

void WidgetTrace::snapshot(Simulation &sim)
{
	const auto &grid = sim.grid;
	if(m_axis < 0 || m_axis >= grid.rank) return;

	int axis_pts = grid.axes[m_axis].points;
	if(axis_pts != m_axis_points) {
		// grid changed, reset history
		m_axis_points = axis_pts;
		m_psi_history.clear();
		m_pot_history.clear();
		m_psi_history.resize(m_history_depth * axis_pts, psi_t(0, 0));
		m_pot_history.resize(m_history_depth * axis_pts, psi_t(0, 0));
		m_history_head = 0;
		m_history_count = 0;
	}

	// read 1D slice/marginal
	std::vector<psi_t> psi_snap(axis_pts);
	std::vector<psi_t> pot_snap(axis_pts);

	if(m_marginal && grid.rank > 1) {
		// marginal: sum over all other axes
		std::vector<float> marg(axis_pts);
		sim.read_marginal_1d(m_axis, marg.data());
		for(int i = 0; i < axis_pts; i++)
			psi_snap[i] = psi_t(sqrtf(marg[i]), 0);
		// no potential for marginal
	} else {
		// slice at current cursor position
		int cursor[MAX_RANK]{};
		for(int i = 0; i < grid.rank; i++)
			cursor[i] = m_view.cursor[i];
		sim.read_slice_1d(m_axis, cursor, psi_snap.data());

		// sample potential at cursor (1D)
		auto pot_view = grid.axis_view(m_axis, cursor, sim.potential.data());
		int idx = 0;
		for(auto val : pot_view) {
			pot_snap[idx++] = val;
			if(idx >= axis_pts) break;
		}
	}

	// write to ring buffer
	psi_t *psi_dest = &m_psi_history[m_history_head * axis_pts];
	psi_t *pot_dest = &m_pot_history[m_history_head * axis_pts];
	std::copy(psi_snap.begin(), psi_snap.end(), psi_dest);
	std::copy(pot_snap.begin(), pot_snap.end(), pot_dest);

	m_history_head = (m_history_head + 1) % m_history_depth;
	if(m_history_count < m_history_depth) m_history_count++;

	if(m_history_count <= 3)
		fprintf(stderr, "trace: snap #%d axis=%d pts=%d |psi[0]|=%.3e\n",
			m_history_count, m_axis, axis_pts, (double)std::abs(psi_snap[axis_pts/2]));
}

void WidgetTrace::update_overlays(SDL_Renderer *rend, int tw, int th, bool horiz)
{
	if(m_history_count == 0 || m_axis_points == 0) return;

	for(int oi = 0; oi < N_OVERLAYS; oi++) {
		auto &ov = m_overlays[oi];
		if(ov.source == DataSource::Off) continue;
		ensure_texture(rend, ov, tw, th);

		// unroll ring buffer into linear array for rendering
		// render_texture layout: buf[x * th + y], x = pixel col, y = pixel row (flipped)
		std::vector<psi_t> psi_buf(tw * th, psi_t(0, 0));
		std::vector<psi_t> pot_buf(tw * th, psi_t(0, 0));

		int samples = m_history_count;
		for(int t = 0; t < samples; t++) {
			int ring_idx = (m_history_head - samples + t + m_history_depth) % m_history_depth;
			psi_t *psi_src = &m_psi_history[ring_idx * m_axis_points];
			psi_t *pot_src = &m_pot_history[ring_idx * m_axis_points];

			if(horiz) {
				// X-axis mode: space horizontal (x), time vertical (y)
				// newest on top → newest = highest y in buffer (render_texture flips)
				for(int s = 0; s < m_axis_points; s++) {
					psi_buf[s * th + t] = psi_src[s];
					pot_buf[s * th + t] = pot_src[s];
				}
			} else {
				// Y-axis mode: time horizontal (x), space vertical (y)
				// newest on right → newest = highest x
				for(int s = 0; s < m_axis_points; s++) {
					psi_buf[t * th + s] = psi_src[s];
					pot_buf[t * th + s] = pot_src[s];
				}
			}
		}

		render_texture(ov, psi_buf.data(), pot_buf.data(), tw, th);
	}
}

void WidgetTrace::draw_controls(const Grid &grid)
{
	ImGui::Text("Trace");
	ImGui::Separator();

	ImGui::AxisCombo("Axis", &m_axis, grid);
	ImGui::Checkbox("Marginal", &m_marginal);

	ImGui::SliderFloat("Interval", &m_snapshot_interval, 0.01f, 1.0f, "%.2fs");
	if(ImGui::SliderInt("History", &m_history_depth, 100, 1000)) {
		// resize buffers
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

	ImGui::Separator();
	draw_overlay_controls(m_overlays, N_OVERLAYS);
}

void WidgetTrace::draw_cursor(SDL_Renderer *rend, bool horiz)
{
	if(m_axis_points < 1) return;

	// LMB drag updates cursor on the traced axis
	if(ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ImVec2 mp = ImGui::GetMousePos();
		if(mp.x >= m_dst.x && mp.x < m_dst.x + m_dst.w &&
		   mp.y >= m_dst.y && mp.y < m_dst.y + m_dst.h) {
			if(horiz) {
				// space is horizontal
				float frac = (mp.x - m_dst.x) / m_dst.w;
				m_view.cursor[m_axis] = (int)(frac * m_axis_points);
			} else {
				// space is vertical (y increases upward → flip)
				float frac = 1.0f - (mp.y - m_dst.y) / m_dst.h;
				m_view.cursor[m_axis] = (int)(frac * m_axis_points);
			}
			if(m_view.cursor[m_axis] < 0) m_view.cursor[m_axis] = 0;
			if(m_view.cursor[m_axis] >= m_axis_points) m_view.cursor[m_axis] = m_axis_points - 1;
		}
	}

	// draw cursor line
	SDL_SetRenderDrawColor(rend, colors::cursor_cross.r, colors::cursor_cross.g,
	                       colors::cursor_cross.b, colors::cursor_cross.a);
	int c = m_view.cursor[m_axis];
	if(horiz) {
		// vertical line at cursor position
		float sx = m_dst.x + ((float)c + 0.5f) / m_axis_points * m_dst.w;
		SDL_RenderLine(rend, sx, m_dst.y, sx, m_dst.y + m_dst.h);
	} else {
		// horizontal line at cursor position (y flipped)
		float sy = m_dst.y + (1.0f - ((float)c + 0.5f) / m_axis_points) * m_dst.h;
		SDL_RenderLine(rend, m_dst.x, sy, m_dst.x + m_dst.w, sy);
	}
}

void WidgetTrace::do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r)
{
	if(exp.simulations.empty()) return;
	auto &sim = *exp.simulations[0];
	const auto &grid = sim.grid;

	// check if it's time to snapshot
	auto now = std::chrono::steady_clock::now();
	if(!m_started) {
		m_last_snapshot = now;
		m_started = true;
	}
	auto elapsed = std::chrono::duration<double>(now - m_last_snapshot).count();
	if(elapsed >= m_snapshot_interval) {
		snapshot(sim);
		m_last_snapshot = now;
	}

	// draw controls
	draw_controls(grid);
	float ctrl_h = ImGui::GetCursorPosY();

	// draw texture
	if(m_axis_points > 0) {
		// orient space axis to match its spatial dimension: x → horizontal, y → vertical
		bool horiz = (sim.cs.dim_of(m_axis) == 0);
		int tw = horiz ? m_axis_points   : m_history_depth;
		int th = horiz ? m_history_depth : m_axis_points;
		update_overlays(rend, tw, th, horiz);

		float avail_w = r.w;
		float avail_h = r.h - ctrl_h - 10;
		float aspect = (float)tw / th;
		SDL_FRect dst;
		dst.w = avail_w;
		dst.h = avail_w / aspect;
		if(dst.h > avail_h) {
			dst.h = avail_h;
			dst.w = avail_h * aspect;
		}
		dst.x = r.x + (avail_w - dst.w) * 0.5f;
		dst.y = r.y + ctrl_h + (avail_h - dst.h) * 0.5f;

		for(auto &ov : m_overlays) {
			if(ov.source == DataSource::Off || !ov.tex) continue;
			SDL_SetTextureAlphaMod(ov.tex, (uint8_t)(ov.opacity * 255));
			SDL_RenderTexture(rend, ov.tex, nullptr, &dst);
		}

		// border
		SDL_SetRenderDrawColor(rend, colors::grid_border.r, colors::grid_border.g,
		                       colors::grid_border.b, colors::grid_border.a);
		SDL_RenderRect(rend, &dst);

		m_dst = dst;
		draw_cursor(rend, horiz);
	}

	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A)) {
		// reset view
		m_psi_history.clear();
		m_pot_history.clear();
		m_psi_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
		m_pot_history.resize(m_history_depth * m_axis_points, psi_t(0, 0));
		m_history_head = 0;
		m_history_count = 0;
	}
}


REGISTER_WIDGET(WidgetTrace,
	.name = "trace",
	.description = "Time trace viewer",
	.hotkey = ImGuiKey_F4,
)
