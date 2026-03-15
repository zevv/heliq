
#include <imgui.h>
#include <SDL3/SDL.h>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"
#include "math3d.hpp"
#include "grid.hpp"
#include "constants.hpp"
#include "misc.hpp"
#include "glview.hpp"

static constexpr double PITCH_LIMIT = M_PI * 0.49;

enum class Envelope { Amplitude, ProbDensity, Real, Imaginary, COUNT };
enum class HelixColor { Gray, Rainbow, Flame, COUNT };

static const char *envelope_names[] = { "|psi|", "|psi|^2", "Re(psi)", "Im(psi)" };
static const char *helix_color_names[] = { "gray", "rainbow", "flame" };


class WidgetHelixGL : public Widget {

public:
	WidgetHelixGL(Widget::Info &info);
	~WidgetHelixGL() override;

private:
	void do_save(ConfigWriter &cfg) override;
	void do_load(ConfigReader::Node *node) override;
	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override;

	enum SliceMode { Slice, Marginal, Momentum };

	// state
	double m_yaw{0}, m_pitch{0}, m_dist{2.5};
	double m_pan_x{0}, m_pan_y{0};
	bool m_ortho{true};
	float m_amplitude{0.1f};

	// visual layers
	bool m_envelope_on{true};
	int m_envelope{0};
	float m_envelope_alpha{0.7f};
	bool m_helix_on{true};
	int m_helix_color{0};
	float m_helix_alpha{1.0f};
	bool m_surface{true};
	float m_surface_alpha{0.1f};
	bool m_potential_on{true};
	float m_potential_alpha{0.3f};
	int m_slice_axis{0};
	int m_slice_pos[MAX_RANK]{};
	int m_slice_mode{Slice};
	bool m_normalize_slice{false};
	bool m_auto_slice{false};
	bool m_orbiting{false}, m_panning{false};
	float m_drag_x{}, m_drag_y{};
	float m_cursor_val{0};
	bool m_cursor_valid{false};

	// data
	std::vector<std::complex<double>> m_slice;
	std::vector<std::complex<double>> m_fft_buf;
	std::vector<double> m_marginals[MAX_RANK];  // probability marginals per axis
	int m_marginal_peak[MAX_RANK]{};            // argmax per axis
	fftw_plan m_fft_plan{};
	int m_fft_n{0};

	// GL
	GLView m_gl;
	std::vector<float> m_vbuf;  // temp vertex buffer

	// data extraction (same as widget-helix.cpp)
	void clamp_slice_positions(const Simulation &sim);
	void extract_data(const Simulation &sim, const std::complex<double> *psi_all, int n);
	void extract_slice(const Simulation &sim, const std::complex<double> *psi_all, int n);
	void extract_marginal(const Simulation &sim, const std::complex<double> *psi_all, int n);
	void extract_momentum(const Simulation &sim, const std::complex<double> *psi_all, int n);
	void compute_marginals(const Simulation &sim, const std::complex<double> *psi_all);
	double compute_max_amp(const Simulation &sim, const std::complex<double> *psi,
	                       const std::complex<double> *psi_all, int n);
	double envelope_value(std::complex<double> psi, double max_amp);

	// camera
	mat4 build_camera(int w, int h);
	void mvp_to_float(const mat4 &m, float *out);

	// GL drawing
	void gl_draw_axis(const mat4 &vp);
	void gl_draw_surface(const std::complex<double> *psi, double max_amp, int n);
	void gl_draw_helix(const std::complex<double> *psi, double max_amp, int n);
	void gl_draw_envelope(const std::complex<double> *psi, double max_amp, int n,
	                       const mat4 &vp);
	void gl_draw_potentials(const Simulation &sim, int n);
	void gl_draw_absorb_zones(const Simulation &sim, int n);
	void gl_draw_cursor();

	// SDL drawing (overlays on top of GL texture)
	void draw_controls(const Simulation &sim);

	// input
	void handle_mouse(SDL_Rect &r);
	void handle_keys();
	float screen_x_to_axis(const mat4 &vp, int w, int h, float sx);
};



WidgetHelixGL::WidgetHelixGL(Widget::Info &info)
	: Widget(info)
{
}


WidgetHelixGL::~WidgetHelixGL()
{
	if(m_fft_plan) fftw_destroy_plan(m_fft_plan);
}


void WidgetHelixGL::do_save(ConfigWriter &cfg)
{
	cfg.write("yaw", m_yaw);
	cfg.write("pitch", m_pitch);
	cfg.write("dist", m_dist);
	cfg.write("pan_x", m_pan_x);
	cfg.write("pan_y", m_pan_y);
	cfg.write("ortho", m_ortho);
	cfg.write("amplitude", m_amplitude);
	cfg.write("envelope_on", m_envelope_on ? 1 : 0);
	cfg.write("envelope", m_envelope);
	cfg.write("envelope_alpha", m_envelope_alpha);
	cfg.write("helix_on", m_helix_on ? 1 : 0);
	cfg.write("helix_color", m_helix_color);
	cfg.write("helix_alpha", m_helix_alpha);
	cfg.write("surface", m_surface ? 1 : 0);
	cfg.write("surface_alpha", m_surface_alpha);
	cfg.write("potential_on", m_potential_on ? 1 : 0);
	cfg.write("potential_alpha", m_potential_alpha);
	cfg.write("slice_axis", m_slice_axis);
	cfg.write("slice_mode", m_slice_mode);
	cfg.write("normalize_slice", m_normalize_slice ? 1 : 0);
	cfg.write("auto_slice", m_auto_slice ? 1 : 0);
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
		cfg.write(key, m_slice_pos[d]);
	}
}


void WidgetHelixGL::do_load(ConfigReader::Node *node)
{
	if(!node) return;
	node->read("yaw", m_yaw);
	node->read("pitch", m_pitch);
	node->read("dist", m_dist);
	node->read("pan_x", m_pan_x);
	node->read("pan_y", m_pan_y);
	node->read("ortho", m_ortho);
	node->read("amplitude", m_amplitude);
	int env_on = m_envelope_on; node->read("envelope_on", env_on); m_envelope_on = env_on;
	node->read("envelope", m_envelope);
	node->read("envelope_alpha", m_envelope_alpha);
	int hx_on = m_helix_on; node->read("helix_on", hx_on); m_helix_on = hx_on;
	node->read("helix_color", m_helix_color);
	node->read("helix_alpha", m_helix_alpha);
	int surf = m_surface; node->read("surface", surf); m_surface = surf;
	node->read("surface_alpha", m_surface_alpha);
	int pot_on = m_potential_on; node->read("potential_on", pot_on); m_potential_on = pot_on;
	node->read("potential_alpha", m_potential_alpha);
	node->read("slice_axis", m_slice_axis);
	node->read("slice_mode", m_slice_mode);
	int ns = 0; node->read("normalize_slice", ns); m_normalize_slice = ns;
	int as = 0; node->read("auto_slice", as); m_auto_slice = as;
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
		node->read(key, m_slice_pos[d]);
	}
}


void WidgetHelixGL::do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r)
{
	if(!m_gl.valid()) m_gl.init(rend);
	if(!m_gl.valid()) {
		ImGui::Text("GL not available");
		return;
	}
	if(exp.simulations.empty()) {
		ImGui::Text("No simulation");
		return;
	}

	auto &sim = *exp.simulations[0];
	if(sim.grid.rank < 1) return;

	auto *psi_all = sim.psi_front();
	if(m_slice_axis >= sim.grid.rank) m_slice_axis = 0;
	int n = sim.grid.axes[m_slice_axis].points;

	clamp_slice_positions(sim);
	if(sim.grid.rank > 1) compute_marginals(sim, psi_all);
	extract_data(sim, psi_all, n);

	auto *psi = m_slice.data();
	double max_amp = compute_max_amp(sim, psi, psi_all, n);

	handle_mouse(r);

	// GL render
	m_gl.resize(r.w, r.h);
	m_gl.begin(rend);

	glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glLineWidth(2.0f);

	mat4 vp = build_camera(r.w, r.h);
	float mvp[16];
	mvp_to_float(vp, mvp);
	m_gl.set_mvp(mvp);

	if(m_slice_mode == Slice && m_potential_on) {
		gl_draw_potentials(sim, n);
		gl_draw_absorb_zones(sim, n);
	}
	if(m_surface) gl_draw_surface(psi, max_amp, n);
	gl_draw_axis(vp);
	if(m_helix_on) gl_draw_helix(psi, max_amp, n);
	if(m_envelope_on) gl_draw_envelope(psi, max_amp, n, vp);
	gl_draw_cursor();

	m_gl.end(rend);

	// need to restore SDL renderer state after GL context switch
	SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

	// blit GL texture
	SDL_FRect dst = { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
	SDL_RenderTexture(rend, m_gl.texture(), nullptr, &dst);

	// cursor detection (needs screen coords)
	{
		ImVec2 mp = ImGui::GetMousePos();
		bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
		               mp.y >= r.y && mp.y < r.y + r.h;
		if(in_rect) {
			m_cursor_val = screen_x_to_axis(vp, r.w, r.h, mp.x - r.x);
			if(m_cursor_val < -1.0f) m_cursor_val = -1.0f;
			if(m_cursor_val >  1.0f) m_cursor_val =  1.0f;
			m_cursor_valid = true;
		} else {
			m_cursor_valid = false;
		}
	}

	if(ImGui::IsWindowFocused()) handle_keys();
	draw_controls(sim);
}


// --- data extraction (identical to widget-helix.cpp) ---


void WidgetHelixGL::clamp_slice_positions(const Simulation &sim)
{
	for(int d = 0; d < sim.grid.rank; d++) {
		if(m_slice_pos[d] == 0 && d != m_slice_axis)
			m_slice_pos[d] = sim.grid.axes[d].points / 2;
		if(m_slice_pos[d] >= sim.grid.axes[d].points)
			m_slice_pos[d] = sim.grid.axes[d].points - 1;
	}
}

void WidgetHelixGL::compute_marginals(const Simulation &sim, const std::complex<double> *psi_all)
{
	// fast path for rank 2
	if(sim.grid.rank == 2) {
		int n0 = sim.grid.axes[0].points;
		int n1 = sim.grid.axes[1].points;
		m_marginals[0].assign(n0, 0);
		m_marginals[1].assign(n1, 0);
		int s0 = sim.grid.stride[0];  // = n1
		int s1 = sim.grid.stride[1];  // = 1
		for(int i = 0; i < n0; i++) {
			for(int j = 0; j < n1; j++) {
				double v = std::norm(psi_all[i * s0 + j * s1]);
				m_marginals[0][i] += v;
				m_marginals[1][j] += v;
			}
		}
		// find global peak position
		double best = -1;
		m_marginal_peak[0] = n0 / 2;
		m_marginal_peak[1] = n1 / 2;
		for(int i = 0; i < n0; i++) {
			for(int j = 0; j < n1; j++) {
				double v = std::norm(psi_all[i * s0 + j * s1]);
				if(v > best) {
					best = v;
					m_marginal_peak[0] = i;
					m_marginal_peak[1] = j;
				}
			}
		}
	}
}


void WidgetHelixGL::extract_slice(const Simulation &sim, const std::complex<double> *psi_all, int n)
{
	for(int i = 0; i < n; i++) {
		int coords[MAX_RANK]{};
		for(int d = 0; d < sim.grid.rank; d++) coords[d] = m_slice_pos[d];
		coords[m_slice_axis] = i;
		m_slice[i] = psi_all[sim.grid.linear_index(coords)];
	}
}

void WidgetHelixGL::extract_marginal(const Simulation &sim, const std::complex<double> *psi_all, int n)
{
	double dv = 1.0;
	for(int d = 0; d < sim.grid.rank; d++)
		if(d != m_slice_axis) dv *= sim.grid.axes[d].dx();
	auto &marg = m_marginals[m_slice_axis];
	for(int i = 0; i < n; i++)
		m_slice[i] = std::complex<double>(sqrt(marg[i] * dv), 0);
}

void WidgetHelixGL::extract_momentum(const Simulation &sim, const std::complex<double> *psi_all, int n)
{
	if(sim.grid.rank == 1)
		for(int i = 0; i < n; i++) m_slice[i] = psi_all[i];
	else
		extract_slice(sim, psi_all, n);

	if(m_fft_n != n) {
		if(m_fft_plan) fftw_destroy_plan(m_fft_plan);
		m_fft_buf.resize(n);
		m_fft_plan = fftw_plan_dft_1d(n,
			(fftw_complex *)m_slice.data(), (fftw_complex *)m_fft_buf.data(),
			FFTW_FORWARD, FFTW_ESTIMATE);
		m_fft_n = n;
	}
	fftw_execute_dft(m_fft_plan,
		(fftw_complex *)m_slice.data(), (fftw_complex *)m_fft_buf.data());
	for(int i = 0; i < n; i++) m_slice[i] = m_fft_buf[(i + n/2) % n];
}

void WidgetHelixGL::extract_data(const Simulation &sim, const std::complex<double> *psi_all, int n)
{
	m_slice.resize(n);
	switch(m_slice_mode) {
		case Marginal:  extract_marginal(sim, psi_all, n); break;
		case Momentum:  extract_momentum(sim, psi_all, n); break;
		default:
			if(sim.grid.rank == 1)
				for(int i = 0; i < n; i++) m_slice[i] = psi_all[i];
			else extract_slice(sim, psi_all, n);
			break;
	}
}

double WidgetHelixGL::compute_max_amp(const Simulation &sim, const std::complex<double> *psi,
                                       const std::complex<double> *psi_all, int n)
{
	double max_amp = 1e-30;
	if(m_slice_mode != Slice || m_normalize_slice) {
		for(int i = 0; i < n; i++) {
			double a = std::abs(psi[i]);
			if(a > max_amp) max_amp = a;
		}
	} else {
		size_t total = sim.grid.total_points();
		for(size_t i = 0; i < total; i++) {
			double a = std::abs(psi_all[i]);
			if(a > max_amp) max_amp = a;
		}
	}
	return max_amp;
}

double WidgetHelixGL::envelope_value(std::complex<double> psi, double max_amp)
{
	switch((Envelope)m_envelope) {
		case Envelope::Amplitude:   return std::abs(psi) / max_amp;
		case Envelope::ProbDensity: return std::norm(psi) / (max_amp * max_amp);
		case Envelope::Real:        return psi.real() / max_amp;
		case Envelope::Imaginary:   return psi.imag() / max_amp;
		default: return 0;
	}
}


// --- camera ---


mat4 WidgetHelixGL::build_camera(int w, int h)
{
	vec3 center = {m_pan_x, m_pan_y, 0};
	vec3 eye = {
		center.x + m_dist * sin(m_yaw) * cos(m_pitch),
		center.y + m_dist * sin(m_pitch),
		center.z + m_dist * cos(m_yaw) * cos(m_pitch),
	};
	mat4 view = mat4::look_at(eye, center, {0, 1, 0});
	double aspect = (double)w / h;
	mat4 proj = m_ortho
		? mat4::ortho(m_dist * 0.5, aspect, 0.001, 1000.0)
		: mat4::perspective(0.8, aspect, 0.001, 1000.0);
	return proj * view;
}


void WidgetHelixGL::mvp_to_float(const mat4 &m, float *out)
{
	for(int i = 0; i < 16; i++) out[i] = (float)m.m[i];
}


// --- GL drawing ---


void WidgetHelixGL::gl_draw_axis(const mat4 &vp)
{
	float a = m_amplitude;
	glUseProgram(m_gl.solid_shader());
	glEnableVertexAttribArray(0);

	// bounding box edges
	glUniform4f(m_gl.color_loc(), 0.18f, 0.18f, 0.18f, 1.0f);
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
	glUniform4f(m_gl.color_loc(), 0.24f, 0.24f, 0.24f, 1.0f);
	float axis[] = { -1,0,0, 1,0,0 };
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, axis);
	glDrawArrays(GL_LINES, 0, 2);

	// origin cross clamped to bounding box
	glUniform4f(m_gl.color_loc(), 0.31f, 0.31f, 0.31f, 1.0f);
	float cross[] = { 0,-a,0, 0,a,0, 0,0,-a, 0,0,a };
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cross);
	glDrawArrays(GL_LINES, 0, 4);

	glDisableVertexAttribArray(0);
}


void WidgetHelixGL::gl_draw_surface(const std::complex<double> *psi, double max_amp, int n)
{
	// triangle strip: base[i], helix[i], base[i+1], helix[i+1]...
	m_vbuf.resize(n * 2 * 3);  // 2 verts per point, 3 floats each
	for(int i = 0; i < n; i++) {
		float t = (float)i / n;
		float x = -1.0f + 2.0f * t;
		float y = (float)(psi[i].real() / max_amp * m_amplitude);
		float z = (float)(psi[i].imag() / max_amp * m_amplitude);
		m_vbuf[i*6+0] = x; m_vbuf[i*6+1] = 0; m_vbuf[i*6+2] = 0;    // base
		m_vbuf[i*6+3] = x; m_vbuf[i*6+4] = y; m_vbuf[i*6+5] = z;    // helix
	}

	glUseProgram(m_gl.solid_shader());
	glUniform4f(m_gl.color_loc(), 0.3f, 0.4f, 0.7f, m_surface_alpha);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, m_vbuf.data());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, n * 2);

	// stem lines
	std::vector<float> stems(n * 6);
	for(int i = 0; i < n; i++) {
		stems[i*6+0] = m_vbuf[i*6+0]; stems[i*6+1] = 0; stems[i*6+2] = 0;
		stems[i*6+3] = m_vbuf[i*6+3]; stems[i*6+4] = m_vbuf[i*6+4]; stems[i*6+5] = m_vbuf[i*6+5];
	}
	glUniform4f(m_gl.color_loc(), 0.31f, 0.31f, 0.47f, 1.0f);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, stems.data());
	glDrawArrays(GL_LINES, 0, (int)stems.size() / 3);
	glDisableVertexAttribArray(0);
}


void WidgetHelixGL::gl_draw_helix(const std::complex<double> *psi, double max_amp, int n)
{
	// per-vertex colored line strip: 3 pos + 4 color per vertex
	m_vbuf.resize(n * 7);
	for(int i = 0; i < n; i++) {
		float t = (float)i / n;
		float x = -1.0f + 2.0f * t;
		float y = (float)(psi[i].real() / max_amp * m_amplitude);
		float z = (float)(psi[i].imag() / max_amp * m_amplitude);
		m_vbuf[i*7+0] = x;
		m_vbuf[i*7+1] = y;
		m_vbuf[i*7+2] = z;

		float amp = (float)(std::abs(psi[i]) / max_amp);
		float cr, cg, cb;
		switch((HelixColor)m_helix_color) {
			case HelixColor::Gray:
				cr = cg = cb = 0.78f * amp + 0.16f * (1 - amp);
				break;
			case HelixColor::Rainbow: {
				double phase = atan2(psi[i].imag(), psi[i].real());
				double hue = (phase + M_PI) / (2 * M_PI);
				uint8_t rr, gg, bb;
				hsv_to_rgb(hue, 1.0, 1.0, rr, gg, bb);
				cr = (rr/255.0f) * amp + 0.16f * (1 - amp);
				cg = (gg/255.0f) * amp + 0.16f * (1 - amp);
				cb = (bb/255.0f) * amp + 0.16f * (1 - amp);
				break;
			}
			case HelixColor::Flame:
				cr = amp + 0.16f * (1 - amp);
				cg = fminf(1.0f, amp * 2.0f) * amp + 0.16f * (1 - amp);
				cb = fminf(1.0f, fmaxf(0.0f, amp * 2.0f - 1.0f)) * amp + 0.08f * (1 - amp);
				break;
			default: cr = cg = cb = 0.5f; break;
		}
		m_vbuf[i*7+3] = cr;
		m_vbuf[i*7+4] = cg;
		m_vbuf[i*7+5] = cb;
		m_vbuf[i*7+6] = m_helix_alpha;
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


void WidgetHelixGL::gl_draw_envelope(const std::complex<double> *psi, double max_amp, int n,
                                      const mat4 &vp)
{
	bool rotational = (Envelope)m_envelope == Envelope::Amplitude ||
	                  (Envelope)m_envelope == Envelope::ProbDensity;

	if(rotational) {
		// ghost longitudinal lines
		float ghost_a = m_envelope_alpha * 0.12f;
		glUseProgram(m_gl.solid_shader());
		int n_ghosts = 64;
		m_vbuf.resize(n * 3);
		for(int g = 0; g < n_ghosts; g++) {
			float angle = 2.0f * M_PI * g / n_ghosts;
			float cy = cosf(angle), cz = sinf(angle);
			glUniform4f(m_gl.color_loc(), 0.39f, 0.78f, 0.39f, ghost_a);
			for(int i = 0; i < n; i++) {
				float t = (float)i / n;
				float x = -1.0f + 2.0f * t;
				float a = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
				m_vbuf[i*3+0] = x;
				m_vbuf[i*3+1] = a * cy;
				m_vbuf[i*3+2] = a * cz;
			}
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, m_vbuf.data());
			glDrawArrays(GL_LINE_STRIP, 0, n);
			glDisableVertexAttribArray(0);
		}

		// ghost cross-section circles
		int circle_segs = 24;
		std::vector<float> circle(circle_segs * 3);
		for(int i = 0; i < n; i += 4) {
			float rad = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
			if(rad < 1e-6f) continue;
			float x = -1.0f + 2.0f * i / n;
			for(int s = 0; s < circle_segs; s++) {
				float a = 2.0f * M_PI * s / circle_segs;
				circle[s*3+0] = x;
				circle[s*3+1] = rad * cosf(a);
				circle[s*3+2] = rad * sinf(a);
			}
			glUniform4f(m_gl.color_loc(), 0.39f, 0.78f, 0.39f, ghost_a);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, circle.data());
			glDrawArrays(GL_LINE_LOOP, 0, circle_segs);
			glDisableVertexAttribArray(0);
		}
	}

	// primary envelope line
	bool on_z = (Envelope)m_envelope == Envelope::Imaginary;
	glUseProgram(m_gl.solid_shader());
	glUniform4f(m_gl.color_loc(), 0.39f, 0.78f, 0.39f, m_envelope_alpha);
	m_vbuf.resize(n * 3);
	for(int i = 0; i < n; i++) {
		float t = (float)i / n;
		float x = -1.0f + 2.0f * t;
		float a = (float)(envelope_value(psi[i], max_amp) * m_amplitude);
		m_vbuf[i*3+0] = x;
		m_vbuf[i*3+1] = on_z ? 0 : a;
		m_vbuf[i*3+2] = on_z ? a : 0;
	}
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, m_vbuf.data());
	glDrawArrays(GL_LINE_STRIP, 0, n);
	glDisableVertexAttribArray(0);
}


// draw a box with per-vertex colors using the vcol shader
// each vertex has position (3) + color (4) = 7 floats
// x0_col/x1_col allow gradient along x axis (for fog effect)
static void gl_draw_box(float x0, float x1, float a,
                         const float x0_col[4], const float x1_col[4])
{
	// 6 faces × 4 verts × 7 floats
	// x0 verts use x0_col, x1 verts use x1_col
	// for y/z faces, interpolate between x0_col and x1_col
	const float *c0 = x0_col, *c1 = x1_col;
	float verts[] = {
		// front (z=a)
		x0,-a,a, c0[0],c0[1],c0[2],c0[3],  x1,-a,a, c1[0],c1[1],c1[2],c1[3],
		x0, a,a, c0[0],c0[1],c0[2],c0[3],  x1, a,a, c1[0],c1[1],c1[2],c1[3],
		// back (z=-a)
		x0,-a,-a, c0[0],c0[1],c0[2],c0[3],  x1,-a,-a, c1[0],c1[1],c1[2],c1[3],
		x0, a,-a, c0[0],c0[1],c0[2],c0[3],  x1, a,-a, c1[0],c1[1],c1[2],c1[3],
		// top (y=a)
		x0,a,-a, c0[0],c0[1],c0[2],c0[3],  x1,a,-a, c1[0],c1[1],c1[2],c1[3],
		x0,a, a, c0[0],c0[1],c0[2],c0[3],  x1,a, a, c1[0],c1[1],c1[2],c1[3],
		// bottom (y=-a)
		x0,-a,-a, c0[0],c0[1],c0[2],c0[3],  x1,-a,-a, c1[0],c1[1],c1[2],c1[3],
		x0,-a, a, c0[0],c0[1],c0[2],c0[3],  x1,-a, a, c1[0],c1[1],c1[2],c1[3],
		// left (x=x0)
		x0,-a,-a, c0[0],c0[1],c0[2],c0[3],  x0,-a,a, c0[0],c0[1],c0[2],c0[3],
		x0, a,-a, c0[0],c0[1],c0[2],c0[3],  x0, a,a, c0[0],c0[1],c0[2],c0[3],
		// right (x=x1)
		x1,-a,-a, c1[0],c1[1],c1[2],c1[3],  x1,-a,a, c1[0],c1[1],c1[2],c1[3],
		x1, a,-a, c1[0],c1[1],c1[2],c1[3],  x1, a,a, c1[0],c1[1],c1[2],c1[3],
	};
	for(int f = 0; f < 6; f++) {
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), verts + f * 28);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7*sizeof(float), verts + f * 28 + 3);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}


void WidgetHelixGL::gl_draw_potentials(const Simulation &sim, int n)
{
	auto *pot = sim.potential;
	float a = m_amplitude;

	// find max potential for alpha scaling
	double v_max = 1e-30;
	bool any = false;
	for(int i = 0; i < n; i++) {
		int coords[MAX_RANK]{};
		for(int d = 0; d < sim.grid.rank; d++) coords[d] = m_slice_pos[d];
		coords[m_slice_axis] = i;
		double v = fabs(pot[sim.grid.linear_index(coords)].real());
		if(v > v_max) v_max = v;
		if(v > 0) any = true;
	}
	if(!any) return;

	float dx = 2.0f / n;
	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	for(int i = 0; i < n; i++) {
		int coords[MAX_RANK]{};
		for(int d = 0; d < sim.grid.rank; d++) coords[d] = m_slice_pos[d];
		coords[m_slice_axis] = i;
		double v = fabs(pot[sim.grid.linear_index(coords)].real());
		if(v < 1e-30) continue;

		float alpha = (float)(v / v_max) * m_potential_alpha;
		float x0 = -1.0f + dx * i;
		float x1 = x0 + dx;
		float col[] = { 0.5f, 0.5f, 0.5f, alpha };
		gl_draw_box(x0, x1, a, col, col);
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelixGL::gl_draw_absorb_zones(const Simulation &sim, int n)
{
	if(!sim.absorbing_boundary) return;
	float w = (float)sim.absorb_width;
	float a = m_amplitude;
	float xl = -1.0f + 2.0f * w;
	float xr = 1.0f - 2.0f * w;
	float peak = 1.00f;

	glUseProgram(m_gl.vcol_shader());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	// left: edge opaque, onset transparent
	float col_edge[] = { 0.1f, 0.1f, 0.8f, peak };
	float col_zero[] = { 0.1f, 0.1f, 0.8f, 0.0f };
	gl_draw_box(-1.0f, xl, a, col_edge, col_zero);

	// right: onset transparent, edge opaque
	gl_draw_box(xr, 1.0f, a, col_zero, col_edge);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
}


void WidgetHelixGL::gl_draw_cursor()
{
	if(!m_cursor_valid) return;

	// vertical line at cursor position, in clip space
	// we need to draw in NDC, so temporarily set identity MVP
	float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
	glUseProgram(m_gl.solid_shader());
	glUniformMatrix4fv(m_gl.mvp_loc(), 1, GL_FALSE, identity);
	glUniform4f(m_gl.color_loc(), 0.78f, 0.24f, 0.24f, 0.78f);

	float x = m_cursor_val;  // already in [-1,1] which is NDC
	float line[] = { x, -1, 0, x, 1, 0 };
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, line);
	glDrawArrays(GL_LINES, 0, 2);
	glDisableVertexAttribArray(0);
}


float WidgetHelixGL::screen_x_to_axis(const mat4 &vp, int w, int h, float sx)
{
	// binary search: find world x in [-1,1] that projects to screen x = sx
	float lo = -1.0f, hi = 1.0f;
	for(int iter = 0; iter < 20; iter++) {
		float mid = (lo + hi) * 0.5f;
		vec3 ndc = vp.transform({mid, 0, 0});
		float screen_x = (1.0f + (float)ndc.x) * 0.5f * w;
		if(screen_x < sx) lo = mid; else hi = mid;
	}
	return (lo + hi) * 0.5f;
}


// --- controls (identical to widget-helix.cpp) ---


void WidgetHelixGL::draw_controls(const Simulation &sim)
{
	ImGui::SetNextItemWidth(60);
	ImGui::SliderFloat("##amp", &m_amplitude, 0.0f, 0.2f, "%.3f");
	ImGui::SameLine();

	if(sim.grid.rank > 1) {
		for(int d = 0; d < sim.grid.rank; d++) {
			ImGui::SameLine();
			ImGui::PushID(d);
			char label[8]; snprintf(label, sizeof(label), "%d", d);
			ImGui::RadioButton(label, &m_slice_axis, d);
			ImGui::PopID();
		}
		ImGui::SameLine();
		static const char *mode_names[] = { "Slice", "Marginal", "Momentum" };
		if(ImGui::Button(mode_names[m_slice_mode]))
			m_slice_mode = (m_slice_mode + 1) % 3;
		if(m_slice_mode == Slice) {
			ImGui::SameLine();
			ImGui::Checkbox("Norm", &m_normalize_slice);
			ImGui::SameLine();
			ImGui::Checkbox("Auto", &m_auto_slice);
		}
	} else {
		if(ImGui::Button(m_slice_mode == Momentum ? "Momentum" : "Position"))
			m_slice_mode = (m_slice_mode == Momentum) ? Slice : Momentum;
	}

	if(ImGui::BeginTable("layers", 4, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 30);
		ImGui::TableSetupColumn("en", ImGuiTableColumnFlags_WidthFixed, 20);
		ImGui::TableSetupColumn("alpha", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("cfg", ImGuiTableColumnFlags_WidthFixed, 80);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Surf");
		ImGui::TableNextColumn(); ImGui::Checkbox("##sf_on", &m_surface);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##sf_a", &m_surface_alpha, 0.0f, 0.5f, "%.2f");
		ImGui::TableNextColumn();

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Helix");
		ImGui::TableNextColumn(); ImGui::Checkbox("##hx_on", &m_helix_on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##hx_a", &m_helix_alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##hx", &m_helix_color, helix_color_names, (int)HelixColor::COUNT);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Env");
		ImGui::TableNextColumn(); ImGui::Checkbox("##env_on", &m_envelope_on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##env_a", &m_envelope_alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##env", &m_envelope, envelope_names, (int)Envelope::COUNT);

		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("Pot");
		ImGui::TableNextColumn(); ImGui::Checkbox("##pot_on", &m_potential_on);
		ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##pot_a", &m_potential_alpha, 0.0f, 1.0f, "%.1f");
		ImGui::TableNextColumn();

		ImGui::EndTable();
	}

	if(m_slice_mode != Marginal) {
		if(m_auto_slice && sim.grid.rank > 1) {
			for(int d = 0; d < sim.grid.rank; d++) {
				if(d == m_slice_axis) continue;
				m_slice_pos[d] = m_marginal_peak[d];
				m_view.cursor[d] = m_marginal_peak[d];
			}
		} else {
			for(int d = 0; d < sim.grid.rank; d++)
				m_slice_pos[d] = m_view.cursor[d];
		}
		m_view.add_slice(m_slice_axis, m_slice_pos);
	}

	if(m_cursor_valid) {
		auto &ax = sim.grid.axes[m_slice_axis];
		if(m_slice_mode == Momentum) {
			double L = ax.max - ax.min;
			double dk = 2.0 * M_PI / L;
			int nn = ax.points;
			double k = m_cursor_val * (nn / 2) * dk;
			double p = k * hbar;
			ImGui::Text("k=%.2e 1/m  p=%.2e kg·m/s", k, p);
		} else {
			double x = ax.min + (m_cursor_val * 0.5 + 0.5) * (ax.max - ax.min);
			ImGui::Text("x=%.2e m", x);
		}
	}
}


// --- input (identical to widget-helix.cpp) ---


void WidgetHelixGL::handle_mouse(SDL_Rect &r)
{
	ImVec2 mp = ImGui::GetMousePos();
	bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
	               mp.y >= r.y && mp.y < r.y + r.h;
	bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

	if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		if(shift) m_panning = true;
		else      m_orbiting = true;
		m_drag_x = mp.x; m_drag_y = mp.y;
	}
	if(m_orbiting && ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !shift) {
		m_yaw   -= (mp.x - m_drag_x) * 0.005;
		m_pitch += (mp.y - m_drag_y) * 0.005;
		if(m_pitch >  PITCH_LIMIT) m_pitch =  PITCH_LIMIT;
		if(m_pitch < -PITCH_LIMIT) m_pitch = -PITCH_LIMIT;
		m_drag_x = mp.x; m_drag_y = mp.y;
	}
	if(m_panning && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		double scale = m_dist * 0.002;
		m_pan_x -= (mp.x - m_drag_x) * scale * cos(m_yaw);
		m_pan_y += (mp.y - m_drag_y) * scale;
		m_drag_x = mp.x; m_drag_y = mp.y;
	}
	if(ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		m_orbiting = false; m_panning = false;
	}

	if(in_rect) {
		float wheel = ImGui::GetIO().MouseWheel;
		if(wheel != 0) {
			m_dist *= (1.0 - wheel * 0.1);
			if(m_dist < 0.1) m_dist = 0.1;
			if(m_dist > 50.0) m_dist = 50.0;
		}
	}
}


void WidgetHelixGL::handle_keys()
{
	auto key = [](ImGuiKey numpad, ImGuiKey regular) {
		return ImGui::IsKeyPressed(numpad) || ImGui::IsKeyPressed(regular);
	};
	bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

	if(key(ImGuiKey_Keypad1, ImGuiKey_1)) {
		if(ctrl) { m_yaw = M_PI; m_pitch = 0; }
		else     { m_yaw = 0;    m_pitch = 0; }
	}
	if(key(ImGuiKey_Keypad3, ImGuiKey_3)) {
		if(ctrl) { m_yaw = -M_PI/2; m_pitch = 0; }
		else     { m_yaw =  M_PI/2; m_pitch = 0; }
	}
	if(key(ImGuiKey_Keypad7, ImGuiKey_7)) {
		if(ctrl) { m_yaw = 0; m_pitch = -PITCH_LIMIT; }
		else     { m_yaw = 0; m_pitch =  PITCH_LIMIT; }
	}
	if(key(ImGuiKey_Keypad5, ImGuiKey_5)) { m_ortho = !m_ortho; }

	if(ImGui::IsKeyPressed(ImGuiKey_A)) {
		m_yaw = 0; m_pitch = 0; m_dist = 2.5;
		m_pan_x = 0; m_pan_y = 0;
		m_ortho = true;
		m_amplitude = 0.1f;
		m_surface = true; m_surface_alpha = 0.1f;
		m_potential_on = true; m_potential_alpha = 0.3f;
		m_helix_on = true; m_helix_color = 0; m_helix_alpha = 1.0f;
		m_envelope_on = true; m_envelope = 0; m_envelope_alpha = 0.7f;
		m_slice_mode = Slice;
	}

	if(key(ImGuiKey_Keypad4, ImGuiKey_4)) { m_yaw -= M_PI/12; }
	if(key(ImGuiKey_Keypad6, ImGuiKey_6)) { m_yaw += M_PI/12; }
	if(key(ImGuiKey_Keypad8, ImGuiKey_8)) {
		m_pitch += M_PI/12;
		if(m_pitch > PITCH_LIMIT) m_pitch = PITCH_LIMIT;
	}
	if(key(ImGuiKey_Keypad2, ImGuiKey_2)) {
		m_pitch -= M_PI/12;
		if(m_pitch < -PITCH_LIMIT) m_pitch = -PITCH_LIMIT;
	}
}


REGISTER_WIDGET(WidgetHelixGL,
	.name = "helix",
	.description = "3D helix wavefunction viewer",
	.hotkey = ImGuiKey_F2,
);
