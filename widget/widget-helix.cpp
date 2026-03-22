
#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <complex>
#include <vector>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"
#include "config.hpp"
#include "math3d.hpp"
#include "grid.hpp"
#include "misc.hpp"
#include "glview.hpp"
#include "style.hpp"

#include "camera3d.hpp"

enum class Envelope { Amplitude, ProbDensity, Real, Imaginary, COUNT };
enum class HelixColor { Default, Gray, Rainbow, Flame, COUNT };

static const char *envelope_names[] = { "|psi|", "|psi|^2", "Re(psi)", "Im(psi)" };
static const char *helix_color_names[] = { "default", "gray", "rainbow", "flame" };


class WidgetHelix : public Widget {

public:
	WidgetHelix(Widget::Info &info);
	~WidgetHelix() override;

private:
	void do_save(ConfigWriter &cfg) override;
	void do_load(ConfigReader::Node *node) override;
	void do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r) override;

	enum SliceMode { Slice, Marginal, MODE_COUNT };
	static const char *slice_mode_names[];

	Camera3D m_camera;

	float m_amplitude{0.1f};
	
	struct {
		bool on{true};
		float alpha{0.5f};
		int color{(int)HelixColor::Default};
	} m_surface;
	
	struct {
		bool on{true};
		int color{0};
		float alpha{1.0f};
	} m_helix;

	struct {
		bool on{true};
		int mode{0};
		float alpha{0.7f};
		int color{(int)HelixColor::Default};
	} m_envelope;
	
	struct {
		bool on{true};
		float alpha{0.3f};
	} m_potential;


	double m_global_max{1e-30};

	// slice state
	struct {
		int axis{0};
		int pos[MAX_RANK]{};
		int mode{Slice};
		bool normalize{false};
		bool auto_track{false};
	} m_slice;

	// GL
	GLView m_gl;
	std::vector<float> m_vbuf;  // temp vertex buffer

	// helpers
	double compute_max_amp(const psi_t *psi, int n);
	double envelope_value(psi_t psi, double max_amp);

	// spline helper
	static float catmull_rom(float p0, float p1, float p2, float p3, float t) {
		return p1 + 0.5f * t * (
			(p2 - p0) + t * (
				(2*p0 - 5*p1 + 4*p2 - p3) + t * (
					-p0 + 3*p1 - 3*p2 + p3)));
	}

	// camera
	void mvp_to_float(const mat4 &m, float *out);

	// GL drawing
	void gl_draw_axis(const mat4 &vp);
	void gl_draw_surface(const psi_t *psi, double max_amp, int n);
	void gl_draw_helix(const psi_t *psi, double max_amp, int n);
	void gl_draw_envelope(const psi_t *psi, double max_amp, int n,
	                       const mat4 &vp);
	void gl_draw_potentials(const psi_t *pot, int n);
	void gl_draw_potential_marginal(const psi_t *pot, int n);
	void gl_draw_cursor(const GridMeta &gm);
	std::tuple<float,float,float> color_for_vert(int color_mode, int idx, float amp, const psi_t *psi, float def_r, float def_g, float def_b);

	// SDL drawing (overlays on top of GL texture)
	void draw_controls(SimContext &ctx);
};

const char *WidgetHelix::slice_mode_names[] = { "Slice", "Marginal" };

WidgetHelix::WidgetHelix(Widget::Info &info)
	: Widget(info)
{
}


WidgetHelix::~WidgetHelix()
{
}


void WidgetHelix::do_save(ConfigWriter &cfg)
{
	m_camera.save(cfg);
	cfg.write("amplitude", m_amplitude);
	cfg.write("envelope_on", m_envelope.on ? 1 : 0);
	cfg.write("envelope", m_envelope.mode);
	cfg.write("envelope_alpha", m_envelope.alpha);
	cfg.write("envelope_color", m_envelope.color);
	cfg.write("helix_on", m_helix.on ? 1 : 0);
	cfg.write("helix_color", m_helix.color);
	cfg.write("helix_alpha", m_helix.alpha);
	cfg.write("surface", m_surface.on ? 1 : 0);
	cfg.write("surface_alpha", m_surface.alpha);
	cfg.write("surface_color", m_surface.color);
	cfg.write("potential_on", m_potential.on ? 1 : 0);
	cfg.write("potential_alpha", m_potential.alpha);
	cfg.write("slice_axis", m_slice.axis);
	cfg.write("slice_mode", m_slice.mode);
	cfg.write("normalize_slice", m_slice.normalize ? 1 : 0);
	cfg.write("auto_slice", m_slice.auto_track ? 1 : 0);
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
		cfg.write(key, m_slice.pos[d]);
	}
}


void WidgetHelix::do_load(ConfigReader::Node *node)
{
	if(!node) return;
	m_camera.load(node);
	node->read("amplitude", m_amplitude);
	int env_on = m_envelope.on; node->read("envelope_on", env_on); m_envelope.on = env_on;
	node->read("envelope", m_envelope.mode);
	node->read("envelope_alpha", m_envelope.alpha);
	node->read("envelope_color", m_envelope.color);
	int hx_on = m_helix.on; node->read("helix_on", hx_on); m_helix.on = hx_on;
	node->read("helix_color", m_helix.color);
	node->read("helix_alpha", m_helix.alpha);
	int surf = m_surface.on; node->read("surface", surf); m_surface.on = surf;
	node->read("surface_alpha", m_surface.alpha);
	node->read("surface_color", m_surface.color);
	int pot_on = m_potential.on; node->read("potential_on", pot_on); m_potential.on = pot_on;
	node->read("potential_alpha", m_potential.alpha);
	node->read("slice_axis", m_slice.axis);
	node->read("slice_mode", m_slice.mode);
	int ns = 0; node->read("normalize_slice", ns); m_slice.normalize = ns;
	int as = 0; node->read("auto_slice", as); m_slice.auto_track = as;
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
		node->read(key, m_slice.pos[d]);
	}
}


// local centroid within a window around current cursor position
static int auto_track(const psi_t *psi, int n, int cur)
{
	int win = n / 4;
	int lo = std::max(0, cur - win);
	int hi = std::min(n, cur + win);
	double sum_w = 0, sum_wi = 0;
	for(int i = lo; i < hi; i++) {
		double w = std::norm(psi[i]);
		sum_w += w;
		sum_wi += w * i;
	}
	if(sum_w > 1e-30) {
		int ci = (int)(sum_wi / sum_w + 0.5);
		return std::clamp(ci, 0, n - 1);
	}
	return cur;
}


void WidgetHelix::do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	// sync from shared view when locked (X pan + zoom, not Y)
	if(m_view.lock) {
		m_camera.pan_x = m_view.camera.pan_x;
		m_camera.dist = m_view.camera.dist;
		m_amplitude = m_view.amplitude;
		m_slice.normalize = m_view.normalize;
		m_slice.auto_track = m_view.auto_track;
	}

	if(!m_gl.valid()) m_gl.init(rend);
	if(!m_gl.valid()) {
		ImGui::Text("GL not available");
		return;
	}

	auto &state = ctx.state();
	if(state.grid.rank < 1) {
		ImGui::Text("No simulation");
		return;
	}

	// clamp slice axis
	if(m_slice.axis >= state.grid.rank) m_slice.axis = 0;
	int n = state.grid.axes[m_slice.axis].points;

	// clamp slice positions
	for(int d = 0; d < state.grid.rank; d++) {
		if(m_slice.pos[d] == 0 && d != m_slice.axis)
			m_slice.pos[d] = state.grid.axes[d].points / 2;
		if(m_slice.pos[d] >= state.grid.axes[d].points)
			m_slice.pos[d] = state.grid.axes[d].points - 1;
	}

	// build extraction request
	ExtractionRequest req{};
	req.axes[0] = m_slice.axis;
	req.axes[1] = -1;  // 1D extraction
	req.marginal = (m_slice.mode == Marginal);
	for(int d = 0; d < MAX_RANK; d++)
		req.cursor[d] = m_slice.pos[d];

	// get result from previous frame (before building request so auto-track can update cursor)
	auto *result = state.find(req);

	if(m_slice.auto_track && m_slice.mode == Slice && state.grid.rank > 1
		&& result && !result->psi.empty())
		m_view.cursor[m_slice.axis] = auto_track(result->psi.data(), n, m_view.cursor[m_slice.axis]);

	// rebuild request with updated cursor
	for(int d = 0; d < MAX_RANK; d++)
		req.cursor[d] = m_view.cursor[d];

	// declare interest (for next frame)
	ctx.request(req);

	if(!result) {
		ImGui::Text("Waiting for data...");
		return;
	}

	auto *psi = result->psi.data();
	auto *pot = result->pot.data();
	double max_amp = compute_max_amp(psi, n);

	m_camera.handle_mouse(r);

	// GL render — fixed aspect, width-anchored
	int gl_w = r.w;
	int gl_h = r.w;  // square: helix size depends only on width
	m_gl.resize(gl_w, gl_h);
	m_gl.begin(rend);

	{
		auto c = Style::color(Style::BgGl);
		glClearColor(c.r, c.g, c.b, c.a);
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLineWidth(1.5f * Style::line_width());

	mat4 vp = m_camera.build(gl_w, gl_h);
	float mvp[16];
	mvp_to_float(vp, mvp);
	m_gl.set_mvp(mvp);

	if(m_potential.on) {
		if(m_slice.mode == Slice) {
			gl_draw_potentials(pot, n);
		} else {
			gl_draw_potential_marginal(pot, n);
		}
	}
	if(m_surface.on) gl_draw_surface(psi, max_amp, n);
	gl_draw_axis(vp);
	if(m_helix.on) gl_draw_helix(psi, max_amp, n);
	if(m_envelope.on) gl_draw_envelope(psi, max_amp, n, vp);
	gl_draw_cursor(state.grid);
	glLineWidth(1.0f * Style::line_width());

	m_gl.end(rend);

	// need to restore SDL renderer state after GL context switch
	SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

	// blit GL texture — centered vertically, cropped to panel
	float src_y = (gl_h - r.h) * 0.5f;
	SDL_FRect src = { 0, src_y, (float)gl_w, (float)r.h };
	SDL_FRect dst = { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
	SDL_RenderTexture(rend, m_gl.texture(), &src, &dst);

	// compute visible spatial range as screen fractions
	vec3 p_left  = vp.transform({-1, 0, 0});
	vec3 p_right = vp.transform({ 1, 0, 0});
	float sl = (1.0f + (float)p_left.x)  * 0.5f;
	float sr = (1.0f + (float)p_right.x) * 0.5f;
	m_view.spatial_sl = sl;
	m_view.spatial_sr = sr;

	// hover cursor: mouse screen X → grid index on current axis
	if(ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		float sl_px = sl * r.w + r.x;
		float sr_px = sr * r.w + r.x;
		float mx = ImGui::GetMousePos().x;
		if(fabs(sr_px - sl_px) > 1.0f) {
			float t = (mx - sl_px) / (sr_px - sl_px);
			int ci = (int)(t * (n - 1) + 0.5f);
			if(ci >= 0 && ci < n)
				m_view.cursor[m_slice.axis] = ci;
		}
	}

	if(ImGui::IsWindowFocused()) {
		m_camera.handle_keys();
		if(ImGui::IsKeyPressed(ImGuiKey_A)) {
			m_camera = Camera3D{};
			m_amplitude = 0.1f;
			m_helix = {};
			m_surface = {};
			m_envelope = {};
			m_potential = {};
		}
	}
	draw_controls(ctx);

	// sync back to shared view when locked (X pan + zoom, not Y)
	if(m_view.lock) {
		m_view.camera.pan_x = m_camera.pan_x;
		m_view.camera.dist = m_camera.dist;
		m_view.amplitude = m_amplitude;
		m_view.normalize = m_slice.normalize;
		m_view.auto_track = m_slice.auto_track;
	}
}


// --- helpers ---


double WidgetHelix::compute_max_amp(const psi_t *psi, int n)
{
	// find local max
	double max_amp = 1e-30;
	for(int i = 0; i < n; i++) {
		double a = std::abs(psi[i]);
		if(a > max_amp) max_amp = a;
	}

	if(m_slice.mode == Slice && !m_slice.normalize) {
		// non-normalized: track global max, decay slowly so it stays stable
		if(max_amp > m_global_max) m_global_max = max_amp;
		else m_global_max *= 0.999;  // slow decay
		return m_global_max;
	}
	return max_amp;
}

double WidgetHelix::envelope_value(psi_t psi, double max_amp)
{
	switch((Envelope)m_envelope.mode) {
		case Envelope::Amplitude:   return std::abs(psi) / max_amp;
		case Envelope::ProbDensity: return std::norm(psi) / (max_amp * max_amp);
		case Envelope::Real:        return psi.real() / max_amp;
		case Envelope::Imaginary:   return psi.imag() / max_amp;
		default: return 0;
	}
}


// --- camera ---


void WidgetHelix::mvp_to_float(const mat4 &m, float *out)
{
	for(int i = 0; i < 16; i++) out[i] = (float)m.m[i];
}


// --- GL drawing ---


void WidgetHelix::gl_draw_axis(const mat4 &vp)
{
	float a = m_amplitude;
	glUseProgram(m_gl.solid_shader());
	glEnableVertexAttribArray(0);

	// bounding box edges
	{
		auto c = Style::color(Style::Gridline0);
		glUniform4f(m_gl.color_loc(), c.r, c.g, c.b, c.a);
	}
	float box[] = {
		// bottom face (y=-a)
		-1,-a,-a, 1,-a,-a,  1,-a,-a, 1,-a,a,  1,-a,a, -1,-a,a,  -1,-a,a, -1,-a,-a,
		// top face (y=a)
		-1,a,-a, 1,a,-a,  1,a,-a, 1,a,a,  1,a,a, -1,a,a,  -1,a,a, -1,a,-a,
		// verticals
		-1,-a,-a, -1,a,-a,  1,-a,-a, 1,a,-a,  1,-a,a, 1,a,a,  -1,-a,a, -1,a,a,
	};
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, box);
	glDrawArrays(GL_LINES, 0, 24);

	// x-axis line
	{
		auto c = Style::color(Style::Gridline1);
		glUniform4f(m_gl.color_loc(), c.r, c.g, c.b, c.a);
	}
	float axis[] = { -1,0,0, 1,0,0 };
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, axis);
	glDrawArrays(GL_LINES, 0, 2);

	// origin cross clamped to bounding box
	{
		auto c = Style::color(Style::Gridline2);
		glUniform4f(m_gl.color_loc(), c.r, c.g, c.b, c.a);
	}
	float c = 0.25f * a;
	float cross[] = { 0,-c,0, 0,c,0, 0,0,-c, 0,0,c };
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cross);
	glDrawArrays(GL_LINES, 0, 4);

	glDisableVertexAttribArray(0);
}


// shared color lookup for helix/surface/envelope
// def_r/g/b are the "Default" color at full amplitude
std::tuple<float,float,float> WidgetHelix::color_for_vert(
	int color_mode, int idx, float amp,
	const psi_t *psi,
	float def_r, float def_g, float def_b)
{
	switch((HelixColor)color_mode) {
	case HelixColor::Rainbow: {
		double phase = atan2(psi[idx].imag(), psi[idx].real());
		double hue = (phase + M_PI) / (2 * M_PI);
		uint8_t rr, gg, bb;
		hsv_to_rgb(hue, 1.0, 0.2 + 0.8 * amp, rr, gg, bb);
		return {rr/255.0f, gg/255.0f, bb/255.0f};
	}
	case HelixColor::Flame:
		return {
			amp + 0.16f * (1 - amp),
			fminf(1.0f, amp * 2.0f) * amp + 0.16f * (1 - amp),
			fminf(1.0f, fmaxf(0.0f, amp * 2.0f - 1.0f)) * amp + 0.08f * (1 - amp)
		};
	case HelixColor::Gray:
		return {
			0.78f * amp + 0.16f * (1 - amp),
			0.78f * amp + 0.16f * (1 - amp),
			0.78f * amp + 0.16f * (1 - amp)
		};
	default: // Default
		return {
			def_r * amp + 0.16f * (1 - amp),
			def_g * amp + 0.16f * (1 - amp),
			def_b * amp + 0.16f * (1 - amp)
		};
	}
}


// fixed number of interpolated points between each pair of samples
static const int HELIX_SUBDIVISIONS = 8;


void WidgetHelix::gl_draw_surface(const psi_t *psi, double max_amp, int n)
{
	auto sc = Style::color(Style::SurfaceDefault);
	float inv_max = (float)(1.0 / max_amp);
	float a = m_amplitude;

	auto pos = [&](int i) -> std::array<float,3> {
		float t = (float)i / n;
		return { -1.0f + 2.0f * t,
		         psi[i].real() * inv_max * a,
		         psi[i].imag() * inv_max * a };
	};

	auto col = [&](int i) -> std::array<float,3> {
		float amp = std::abs(psi[i]) * inv_max;
		auto [cr, cg, cb] = color_for_vert(m_surface.color, i, amp, psi, sc.r, sc.g, sc.b);
		return { cr, cg, cb };
	};

	float s = m_surface.alpha;
	float spoke_alpha = (s <= 0.5f) ? s * 2.0f : 1.0f;
	float surf_alpha  = (s <= 0.5f) ? 0.0f : (s - 0.5f);

	// build interpolated triangle strip for surface fill
	int vcount = 0;
	m_vbuf.resize(n * 2 * 7 * (HELIX_SUBDIVISIONS + 2));
	auto emit_pair = [&](float x, float y, float z, float cr, float cg, float cb, float alpha) {
		int base = vcount * 7;
		if(base + 14 > (int)m_vbuf.size()) m_vbuf.resize(m_vbuf.size() * 2);
		m_vbuf[base+0] = x; m_vbuf[base+1] = 0; m_vbuf[base+2] = 0;
		m_vbuf[base+3] = cr*0.5f; m_vbuf[base+4] = cg*0.5f; m_vbuf[base+5] = cb*0.5f;
		m_vbuf[base+6] = alpha;
		m_vbuf[base+7] = x; m_vbuf[base+8] = y; m_vbuf[base+9] = z;
		m_vbuf[base+10] = cr; m_vbuf[base+11] = cg; m_vbuf[base+12] = cb;
		m_vbuf[base+13] = alpha;
		vcount += 2;
	};

	auto fill_strip = [&](float alpha) {
		vcount = 0;
		for(int i = 0; i < n; i++) {
			auto [px, py, pz] = pos(i);
			auto [cr, cg, cb] = col(i);
			emit_pair(px, py, pz, cr, cg, cb, alpha);

			if(i < n - 1) {
				int i0 = (i > 0) ? i - 1 : 0;
				int i3 = (i + 2 < n) ? i + 2 : n - 1;
				auto p0 = pos(i0), p1 = pos(i), p2 = pos(i+1), p3 = pos(i3);
				auto c0 = col(i0), c1 = col(i), c2 = col(i+1), c3 = col(i3);
				for(int s = 1; s <= HELIX_SUBDIVISIONS; s++) {
					float t = (float)s / (HELIX_SUBDIVISIONS + 1);
					emit_pair(
						p1[0] + t * (p2[0] - p1[0]),
						catmull_rom(p0[1], p1[1], p2[1], p3[1], t),
						catmull_rom(p0[2], p1[2], p2[2], p3[2], t),
						catmull_rom(c0[0], c1[0], c2[0], c3[0], t),
						catmull_rom(c0[1], c1[1], c2[1], c3[1], t),
						catmull_rom(c0[2], c1[2], c2[2], c3[2], t),
						alpha);
				}
			}
		}
	};

	int stride = 7 * sizeof(float);
	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	// surface fill (interpolated, smooth)
	if(surf_alpha > 0.0f) {
		fill_strip(surf_alpha);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, m_vbuf.data());
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, m_vbuf.data() + 3);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, vcount);
	}

	// spoke lines (original sample points, no interpolation)
	{
		vcount = 0;
		for(int i = 0; i < n; i++) {
			auto [px, py, pz] = pos(i);
			auto [cr, cg, cb] = col(i);
			emit_pair(px, py, pz, cr, cg, cb, spoke_alpha);
		}
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, m_vbuf.data());
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, m_vbuf.data() + 3);
		glDrawArrays(GL_LINES, 0, vcount);
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelix::gl_draw_helix(const psi_t *psi, double max_amp, int n)
{
	auto hc = Style::color(Style::HelixDefault);
	float inv_max = (float)(1.0 / max_amp);
	float a = m_amplitude;

	auto pos = [&](int i) -> std::array<float,3> {
		float t = (float)i / n;
		return { -1.0f + 2.0f * t,
		         psi[i].real() * inv_max * a,
		         psi[i].imag() * inv_max * a };
	};

	auto col = [&](int i) -> std::array<float,4> {
		float amp = std::abs(psi[i]) * inv_max;
		auto [cr, cg, cb] = color_for_vert(m_helix.color, i, amp, psi, hc.r, hc.g, hc.b);
		return { cr, cg, cb, m_helix.alpha };
	};

	int vcount = 0;
	m_vbuf.resize(n * 7 * (HELIX_SUBDIVISIONS + 2));
	auto emit = [&](float px, float py, float pz, float cr, float cg, float cb, float ca) {
		int base = vcount * 7;
		if(base + 7 > (int)m_vbuf.size()) m_vbuf.resize(m_vbuf.size() * 2);
		m_vbuf[base+0] = px; m_vbuf[base+1] = py; m_vbuf[base+2] = pz;
		m_vbuf[base+3] = cr; m_vbuf[base+4] = cg; m_vbuf[base+5] = cb; m_vbuf[base+6] = ca;
		vcount++;
	};

	for(int i = 0; i < n; i++) {
		auto [px, py, pz] = pos(i);
		auto [cr, cg, cb, ca] = col(i);
		emit(px, py, pz, cr, cg, cb, ca);

		if(i < n - 1) {
			int i0 = (i > 0) ? i - 1 : 0;
			int i3 = (i + 2 < n) ? i + 2 : n - 1;
			auto p0 = pos(i0), p1 = pos(i), p2 = pos(i+1), p3 = pos(i3);
			auto c0 = col(i0), c1 = col(i), c2 = col(i+1), c3 = col(i3);
			for(int s = 1; s <= HELIX_SUBDIVISIONS; s++) {
				float t = (float)s / (HELIX_SUBDIVISIONS + 1);
				emit(
					p1[0] + t * (p2[0] - p1[0]),
					catmull_rom(p0[1], p1[1], p2[1], p3[1], t),
					catmull_rom(p0[2], p1[2], p2[2], p3[2], t),
					catmull_rom(c0[0], c1[0], c2[0], c3[0], t),
					catmull_rom(c0[1], c1[1], c2[1], c3[1], t),
					catmull_rom(c0[2], c1[2], c2[2], c3[2], t),
					catmull_rom(c0[3], c1[3], c2[3], c3[3], t));
			}
		}
	}

	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data());
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data() + 3);
	glDrawArrays(GL_LINE_STRIP, 0, vcount);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelix::gl_draw_envelope(const psi_t *psi, double max_amp, int n,
                                      const mat4 &vp)
{
	auto ec = Style::color(Style::EnvelopeDefault);
	bool rotational = (Envelope)m_envelope.mode == Envelope::Amplitude ||
	                  (Envelope)m_envelope.mode == Envelope::ProbDensity;

	if(rotational) {
		// ghost longitudinal lines with per-vertex color
		float ghost_a = m_envelope.alpha * 0.12f;
		int n_ghosts = 64;
		m_vbuf.resize(n * 7);
		glUseProgram(m_gl.vcol_shader());
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		for(int g = 0; g < n_ghosts; g++) {
			float angle = 2.0f * M_PI * g / n_ghosts;
			float cy = cosf(angle), cz = sinf(angle);
			for(int i = 0; i < n; i++) {
				float t = (float)i / n;
				float x = -1.0f + 2.0f * t;
				float a = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
				float amp = (float)(std::abs(psi[i]) / max_amp);
				auto [cr, cg, cb] = color_for_vert(m_envelope.color, i, amp, psi, ec.r, ec.g, ec.b);
				m_vbuf[i*7+0] = x;
				m_vbuf[i*7+1] = a * cy;
				m_vbuf[i*7+2] = a * cz;
				m_vbuf[i*7+3] = cr;
				m_vbuf[i*7+4] = cg;
				m_vbuf[i*7+5] = cb;
				m_vbuf[i*7+6] = ghost_a;
			}
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data());
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data() + 3);
			glDrawArrays(GL_LINE_STRIP, 0, n);
		}
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(0);

		// ghost cross-section circles — use color at that x position
		int circle_segs = 24;
		std::vector<float> circle(circle_segs * 7);
		glUseProgram(m_gl.vcol_shader());
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		for(int i = 0; i < n; i += 4) {
			float rad = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
			if(rad < 1e-6f) continue;
			float x = -1.0f + 2.0f * i / n;
			float amp = (float)(std::abs(psi[i]) / max_amp);
			auto [cr, cg, cb] = color_for_vert(m_envelope.color, i, amp, psi, ec.r, ec.g, ec.b);
			for(int s = 0; s < circle_segs; s++) {
				float a = 2.0f * M_PI * s / circle_segs;
				circle[s*7+0] = x;
				circle[s*7+1] = rad * cosf(a);
				circle[s*7+2] = rad * sinf(a);
				circle[s*7+3] = cr;
				circle[s*7+4] = cg;
				circle[s*7+5] = cb;
				circle[s*7+6] = ghost_a;
			}
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), circle.data());
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), circle.data() + 3);
			glDrawArrays(GL_LINE_LOOP, 0, circle_segs);
		}
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(0);
	}

	// primary envelope line with per-vertex color
	bool on_z = (Envelope)m_envelope.mode == Envelope::Imaginary;
	m_vbuf.resize(n * 7);
	for(int i = 0; i < n; i++) {
		float t = (float)i / n;
		float x = -1.0f + 2.0f * t;
		float a = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
		float amp = (float)(std::abs(psi[i]) / max_amp);
		auto [cr, cg, cb] = color_for_vert(m_envelope.color, i, amp, psi, ec.r, ec.g, ec.b);
		m_vbuf[i*7+0] = x;
		m_vbuf[i*7+1] = on_z ? 0 : a;
		m_vbuf[i*7+2] = on_z ? a : 0;
		m_vbuf[i*7+3] = cr;
		m_vbuf[i*7+4] = cg;
		m_vbuf[i*7+5] = cb;
		m_vbuf[i*7+6] = m_envelope.alpha;
	}
	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data());
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), m_vbuf.data() + 3);
	glDrawArrays(GL_LINE_STRIP, 0, n);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


// draw a box with per-vertex colors using the vcol shader
// each vertex has position (3) + color (4) = 7 floats
// x0_col/x1_col allow gradient along x axis (for fog effect)
static void gl_draw_box(float x0, float x1, float a,
                         const float x0_col[4], const float x1_col[4])
{
	// tube: 4 faces wrapping around X (front, back, top, bottom)
	// no left/right caps — the wavefunction passes through
	const float *c0 = x0_col, *c1 = x1_col;
	float faces[] = {
		// front (z=+a)
		x0,-a,a, c0[0],c0[1],c0[2],c0[3],  x1,-a,a, c1[0],c1[1],c1[2],c1[3],
		x0, a,a, c0[0],c0[1],c0[2],c0[3],  x1, a,a, c1[0],c1[1],c1[2],c1[3],
		// back (z=-a)
		x0,-a,-a, c0[0],c0[1],c0[2],c0[3],  x1,-a,-a, c1[0],c1[1],c1[2],c1[3],
		x0, a,-a, c0[0],c0[1],c0[2],c0[3],  x1, a,-a, c1[0],c1[1],c1[2],c1[3],
		// top (y=+a)
		x0,a,-a, c0[0],c0[1],c0[2],c0[3],  x1,a,-a, c1[0],c1[1],c1[2],c1[3],
		x0,a, a, c0[0],c0[1],c0[2],c0[3],  x1,a, a, c1[0],c1[1],c1[2],c1[3],
		// bottom (y=-a)
		x0,-a,-a, c0[0],c0[1],c0[2],c0[3],  x1,-a,-a, c1[0],c1[1],c1[2],c1[3],
		x0,-a, a, c0[0],c0[1],c0[2],c0[3],  x1,-a, a, c1[0],c1[1],c1[2],c1[3],
	};
	for(int f = 0; f < 4; f++) {
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), faces + f * 28);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), faces + f * 28 + 3);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

}


static void gl_draw_cap(float x, float a, const float col[4])
{
	float cap[] = {
		x,-a,-a, col[0],col[1],col[2],col[3],  x,-a,a, col[0],col[1],col[2],col[3],
		x, a,-a, col[0],col[1],col[2],col[3],  x, a,a, col[0],col[1],col[2],col[3],
	};
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), cap);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), cap + 3);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void WidgetHelix::gl_draw_potentials(const psi_t *pot, int n)
{
	auto pc = Style::color(Style::PotentialMarginal);
	float a = m_amplitude;

	// find max potential for alpha scaling
	double v_max = 1e-30;
	bool any = false;
	for(int i = 0; i < n; i++) {
		double v = fabs(pot[i].real());
		if(v > v_max) v_max = v;
		if(v > 0) any = true;
	}
	if(!any) return;

	float dx = 2.0f / n;
	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	bool prev_on = false;
	float prev_alpha = 0;
	for(int i = 0; i < n; i++) {
		double v = fabs(pot[i].real());
		float alpha = (v > 1e-30) ? (float)(v / v_max) * m_potential.alpha : 0;
		bool on = alpha > 1e-4f;

		if(on) {
			float x0 = -1.0f + dx * i;
			float x1 = x0 + dx;
			float col[] = { pc.r, pc.g, pc.b, alpha };
			gl_draw_box(x0, x1, a, col, col);

			// cap at rising edge
			if(!prev_on)
				gl_draw_cap(x0, a, col);
		}

		// cap at falling edge
		if(prev_on && !on) {
			float px1 = -1.0f + dx * i;
			float pcol[] = { pc.r, pc.g, pc.b, prev_alpha };
			gl_draw_cap(px1, a, pcol);
		}

		prev_on = on;
		prev_alpha = alpha;
	}

	// cap at domain edge if last cell has potential
	if(prev_on) {
		float pcol[] = { pc.r, pc.g, pc.b, prev_alpha };
		gl_draw_cap(1.0f, a, pcol);
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelix::gl_draw_potential_marginal(const psi_t *pot, int n)
{
	auto pc = Style::color(Style::PotentialMarginal);
	float a = m_amplitude;

	double v_max = 1e-30;
	bool any = false;
	for(int i = 0; i < n; i++) {
		double v = fabs(pot[i].real());
		if(v > v_max) v_max = v;
		if(v > 0) any = true;
	}
	if(!any) return;

	float dx = 2.0f / n;
	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	float prev_alpha = 0;
	bool prev_on = false;
	for(int i = 0; i < n; i++) {
		double v = fabs(pot[i].real());
		float alpha = (v > 1e-30) ? (float)(v / v_max) * m_potential.alpha : 0;
		bool on = alpha > 1e-4f;
		float x = -1.0f + dx * i;

		// cap at any significant alpha change (not just on/off)
		float delta = alpha - prev_alpha;
		if(on && (!prev_on || delta > 0.05f * m_potential.alpha)) {
			float col[] = { pc.r, pc.g, pc.b, alpha };
			gl_draw_cap(x, a, col);
		}
		if((prev_on && !on) || (prev_on && on && delta < -0.05f * m_potential.alpha)) {
			float pcol[] = { pc.r, pc.g, pc.b, prev_alpha };
			gl_draw_cap(x, a, pcol);
		}

		if(on) {
			float x1 = x + dx;
			float col[] = { pc.r, pc.g, pc.b, alpha };
			gl_draw_box(x, x1, a, col, col);
		}

		prev_on = on;
		prev_alpha = alpha;
	}

	if(prev_on) {
		float pcol[] = { pc.r, pc.g, pc.b, prev_alpha };
		gl_draw_cap(1.0f, a, pcol);
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelix::gl_draw_cursor(const GridMeta &gm)
{
	int ax = m_slice.axis;
	int n = gm.axes[ax].points;
	int ci = m_view.cursor[ax];
	if(ci < 0 || ci >= n) return;

	// map grid index to world x in [-1, 1]; 1 pixel wide slab
	float x = (float)ci / (n - 1) * 2.0f - 1.0f;
	float s = 1.0f / (n - 1);  // half-pixel width
	float a = m_amplitude;

	glUseProgram(m_gl.solid_shader());
	glEnableVertexAttribArray(0);

	// outline only
	{
		auto c = Style::color(Style::CursorEdge);
		glUniform4f(m_gl.color_loc(), c.r, c.g, c.b, c.a);
	}
	float outline[] = {
		x, -a, -a,  x, a, -a,
		x,  a, -a,  x, a,  a,
		x,  a,  a,  x, -a, a,
		x, -a,  a,  x, -a, -a,
	};
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, outline);
	glDrawArrays(GL_LINES, 0, 8);

	glDisableVertexAttribArray(0);
}


// --- controls ---


void WidgetHelix::draw_controls(SimContext &ctx)
{
	auto &state = ctx.state();

	ImGui::SameLine();
	ImGui::ToggleButton("L", &m_view.lock);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(60);
	ImGui::SliderFloat("##amp", &m_amplitude, 0.0f, 0.2f, "%.3f");

	if(state.grid.rank > 1) {
		ImGui::SameLine();
		// Inline axis combo
		const char *preview = (m_slice.axis >= 0 && m_slice.axis < state.grid.rank && state.grid.axes[m_slice.axis].label[0])
			? state.grid.axes[m_slice.axis].label : "?";
		ImGui::SetNextItemWidth(60);
		if(ImGui::BeginCombo("##slice_ax", preview, ImGuiComboFlags_NoArrowButton)) {
			for(int d = 0; d < state.grid.rank; d++) {
				const char *lbl = state.grid.axes[d].label[0] ? state.grid.axes[d].label : nullptr;
				char fb[8]; snprintf(fb, sizeof(fb), "%d", d);
				bool selected = (m_slice.axis == d);
				if(ImGui::Selectable(lbl ? lbl : fb, selected))
					m_slice.axis = d;
				if(selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::Combo("##mode", &m_slice.mode, slice_mode_names, MODE_COUNT);
	}
	if(m_slice.mode == Slice) {
		ImGui::SameLine();
		ImGui::Checkbox("Norm", &m_slice.normalize);
		if(state.grid.rank > 1) {
			ImGui::SameLine();
			ImGui::Checkbox("Auto", &m_slice.auto_track);
		}
	}

	// Measure button + M key (right-aligned)
	int mact = ImGui::MeasureButton();
	if(mact == 1) ctx.push(CmdMeasure{m_slice.axis});
	if(mact == 2) ctx.push(CmdDecohere{m_slice.axis});

	if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::BeginTable("layers", 5, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 30);
		ImGui::TableSetupColumn("en", ImGuiTableColumnFlags_WidthFixed, 20);
		ImGui::TableSetupColumn("alpha", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("cfg", ImGuiTableColumnFlags_WidthFixed, 80);
		ImGui::TableSetupColumn("col", ImGuiTableColumnFlags_WidthFixed, 80);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Surf");
		ImGui::TableNextColumn(); ImGui::Checkbox("##sf_on", &m_surface.on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##sf_a", &m_surface.alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn();
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##sf_c", &m_surface.color, helix_color_names, (int)HelixColor::COUNT);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Helix");
		ImGui::TableNextColumn(); ImGui::Checkbox("##hx_on", &m_helix.on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##hx_a", &m_helix.alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn();
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##hx", &m_helix.color, helix_color_names, (int)HelixColor::COUNT);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Env");
		ImGui::TableNextColumn(); ImGui::Checkbox("##env_on", &m_envelope.on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##env_a", &m_envelope.alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##env", &m_envelope.mode, envelope_names, (int)Envelope::COUNT);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##env_c", &m_envelope.color, helix_color_names, (int)HelixColor::COUNT);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Pot");
		ImGui::TableNextColumn(); ImGui::Checkbox("##pot_on", &m_potential.on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##pot_a", &m_potential.alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn();
		ImGui::TableNextColumn();

		ImGui::EndTable();
	}

	if(m_slice.mode == Slice) {
		for(int d = 0; d < state.grid.rank; d++)
			m_slice.pos[d] = m_view.cursor[d];
		m_view.add_slice(m_slice.axis, m_slice.pos);
	}
}


REGISTER_WIDGET(WidgetHelix,
	.name = "helix",
	.description = "3D helix wavefunction viewer",
	.hotkey = ImGuiKey_F2,
);
