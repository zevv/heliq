#include <imgui.h>
#include <SDL3/SDL.h>
#include <fftw3.h>
#include <math.h>
#include <complex>
#include <vector>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"
#include "math3d.hpp"
#include "grid.hpp"

enum class Envelope { Off, Amplitude, ProbDensity, Real, Imaginary, COUNT };

static const char *envelope_names[] = { "off", "|psi|", "|psi|^2", "Re(psi)", "Im(psi)" };

class WidgetHelix : public Widget {
public:
	WidgetHelix(Widget::Info &info) : Widget(info) {}
	~WidgetHelix() { if(m_fft_plan) fftw_destroy_plan(m_fft_plan); }

	void do_save(ConfigWriter &cfg) override {
		cfg.write("yaw", m_yaw);
		cfg.write("pitch", m_pitch);
		cfg.write("dist", m_dist);
		cfg.write("pan_x", m_pan_x);
		cfg.write("pan_y", m_pan_y);
		cfg.write("ortho", m_ortho);
		cfg.write("envelope", m_envelope);
		cfg.write("helix_color", m_helix_color);
		cfg.write("amplitude", m_amplitude);
		cfg.write("stem_density", m_stem_density);
		cfg.write("slice_axis", m_slice_axis);
		cfg.write("slice_mode", m_slice_mode);
		for(int d = 0; d < MAX_RANK; d++) {
			char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
			cfg.write(key, m_slice_pos[d]);
		}
	}

	void do_load(ConfigReader::Node *node) override {
		if(!node) return;
		node->read("yaw", m_yaw);
		node->read("pitch", m_pitch);
		node->read("dist", m_dist);
		node->read("pan_x", m_pan_x);
		node->read("pan_y", m_pan_y);
		node->read("ortho", m_ortho);
		node->read("envelope", m_envelope);
		node->read("helix_color", m_helix_color);
		node->read("amplitude", m_amplitude);
		node->read("stem_density", m_stem_density);
		node->read("slice_axis", m_slice_axis);
		node->read("slice_mode", m_slice_mode);
		for(int d = 0; d < MAX_RANK; d++) {
			char key[16]; snprintf(key, sizeof(key), "slice_%d", d);
			node->read(key, m_slice_pos[d]);
		}
	}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
		SDL_SetRenderDrawColor(rend, 10, 10, 15, 255);
		SDL_RenderFillRect(rend, nullptr);

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
		extract_data(sim, psi_all, n);

		auto *psi = m_slice.data();
		double max_amp = compute_max_amp(sim, psi, psi_all, n);

		handle_mouse(r);

		mat4 vp = build_camera(r);
		std::vector<SDL_FPoint> helix_pts(n);
		std::vector<vec3> pts3d(n);
		build_points(psi, max_amp, n, vp, r, helix_pts, pts3d);

		if(m_slice_mode != Momentum)
			draw_potentials(rend, sim, n, vp, r);
		draw_stems(rend, n, vp, r, pts3d, helix_pts);
		draw_axis(rend, vp, r);
		draw_helix(rend, psi, max_amp, n, helix_pts);
		draw_envelope(rend, psi, max_amp, n, vp, r);
		draw_cursor(rend, sim, vp, r, n);

		if(ImGui::IsWindowFocused()) handle_keys();
		draw_controls(sim);
	}

private:
	// --- state ---
	double m_yaw{0};
	double m_pitch{0};
	double m_dist{2.5};
	double m_pan_x{0}, m_pan_y{0};
	bool m_ortho{true};
	int m_envelope{1};
	int m_helix_color{1};
	float m_amplitude{1.0f};
	int m_stem_density{64};
	int m_slice_axis{0};
	int m_slice_pos[MAX_RANK]{};
	std::vector<std::complex<double>> m_slice;
	std::vector<std::complex<double>> m_fft_buf;
	fftw_plan m_fft_plan{};
	int m_fft_n{0};

	enum SliceMode { Slice, Marginal, Momentum };
	int m_slice_mode{Slice};

	bool m_orbiting{false};
	bool m_panning{false};
	float m_drag_x{}, m_drag_y{};


	// --- data extraction ---

	void clamp_slice_positions(const Simulation &sim) {
		for(int d = 0; d < sim.grid.rank; d++) {
			if(m_slice_pos[d] == 0 && d != m_slice_axis)
				m_slice_pos[d] = sim.grid.axes[d].points / 2;
			if(m_slice_pos[d] >= sim.grid.axes[d].points)
				m_slice_pos[d] = sim.grid.axes[d].points - 1;
		}
	}

	void extract_slice(const Simulation &sim, const std::complex<double> *psi_all, int n) {
		for(int i = 0; i < n; i++) {
			int coords[MAX_RANK]{};
			for(int d = 0; d < sim.grid.rank; d++)
				coords[d] = m_slice_pos[d];
			coords[m_slice_axis] = i;
			size_t idx = sim.grid.linear_index(coords);
			m_slice[i] = psi_all[idx];
		}
	}

	void extract_marginal(const Simulation &sim, const std::complex<double> *psi_all, int n) {
		double dv = 1.0;
		for(int d = 0; d < sim.grid.rank; d++)
			if(d != m_slice_axis) dv *= sim.grid.axes[d].dx();
		for(int i = 0; i < n; i++) {
			double prob = 0;
			int coords[MAX_RANK]{};
			auto sum_axis = [&](auto &&self, int dim) -> void {
				if(dim == sim.grid.rank) {
					coords[m_slice_axis] = i;
					size_t idx = sim.grid.linear_index(coords);
					prob += std::norm(psi_all[idx]);
					return;
				}
				if(dim == m_slice_axis) {
					self(self, dim + 1);
					return;
				}
				for(int j = 0; j < sim.grid.axes[dim].points; j++) {
					coords[dim] = j;
					self(self, dim + 1);
				}
			};
			sum_axis(sum_axis, 0);
			m_slice[i] = std::complex<double>(sqrt(prob * dv), 0);
		}
	}

	void extract_momentum(const Simulation &sim, const std::complex<double> *psi_all, int n) {
		// get position-space data first
		if(sim.grid.rank == 1) {
			for(int i = 0; i < n; i++)
				m_slice[i] = psi_all[i];
		} else {
			extract_slice(sim, psi_all, n);
		}

		// ensure FFT plan
		if(m_fft_n != n) {
			if(m_fft_plan) fftw_destroy_plan(m_fft_plan);
			m_fft_buf.resize(n);
			m_fft_plan = fftw_plan_dft_1d(n,
				(fftw_complex *)m_slice.data(),
				(fftw_complex *)m_fft_buf.data(),
				FFTW_FORWARD, FFTW_ESTIMATE);
			m_fft_n = n;
		}

		// FFT position → momentum
		fftw_execute_dft(m_fft_plan,
			(fftw_complex *)m_slice.data(),
			(fftw_complex *)m_fft_buf.data());

		// fftshift: k=0 in center
		for(int i = 0; i < n; i++) {
			int j = (i + n/2) % n;
			m_slice[i] = m_fft_buf[j];
		}
	}

	void extract_data(const Simulation &sim, const std::complex<double> *psi_all, int n) {
		m_slice.resize(n);
		switch(m_slice_mode) {
			case Marginal:
				extract_marginal(sim, psi_all, n);
				break;
			case Momentum:
				extract_momentum(sim, psi_all, n);
				break;
			default:
				if(sim.grid.rank == 1)
					for(int i = 0; i < n; i++) m_slice[i] = psi_all[i];
				else
					extract_slice(sim, psi_all, n);
				break;
		}
	}

	double compute_max_amp(const Simulation &sim, const std::complex<double> *psi,
	                       const std::complex<double> *psi_all, int n) {
		double max_amp = 1e-30;
		if(m_slice_mode != Slice) {
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


	// --- helpers ---

	static SDL_FPoint project_to_screen(vec3 ndc, float rx, float ry, float rw, float rh) {
		return { rx + (1.0f + (float)ndc.x) * 0.5f * rw,
		         ry + (1.0f - (float)ndc.y) * 0.5f * rh };
	}


	// --- camera ---

	mat4 build_camera(SDL_Rect &r) {
		vec3 center = {m_pan_x, m_pan_y, 0};
		vec3 eye = {
			center.x + m_dist * sin(m_yaw) * cos(m_pitch),
			center.y + m_dist * sin(m_pitch),
			center.z + m_dist * cos(m_yaw) * cos(m_pitch),
		};
		mat4 view = mat4::look_at(eye, center, {0, 1, 0});
		double aspect = (double)r.w / r.h;
		mat4 proj = m_ortho
			? mat4::ortho(m_dist * 0.5, aspect, 0.001, 1000.0)
			: mat4::perspective(0.8, aspect, 0.001, 1000.0);
		return proj * view;
	}

	void build_points(const std::complex<double> *psi, double max_amp, int n,
	                  const mat4 &vp, SDL_Rect &r,
	                  std::vector<SDL_FPoint> &helix_pts, std::vector<vec3> &pts3d) {
		for(int i = 0; i < n; i++) {
			double t = (double)i / (n - 1);
			double x = -1.0 + 2.0 * t;
			double y = psi[i].real() / max_amp * m_amplitude;
			double z = psi[i].imag() / max_amp * m_amplitude;
			pts3d[i] = {x, y, z};
			vec3 ndc = vp.transform(pts3d[i]);
			helix_pts[i] = project_to_screen(ndc, r.x, r.y, r.w, r.h);
		}
	}


	// --- drawing ---

	void draw_axis(SDL_Renderer *rend, const mat4 &vp, SDL_Rect &r) {
		vec3 a = vp.transform({-1, 0, 0});
		vec3 b = vp.transform({ 1, 0, 0});
		SDL_FPoint axis[2] = {
			project_to_screen(a, r.x, r.y, r.w, r.h),
			project_to_screen(b, r.x, r.y, r.w, r.h),
		};
		SDL_SetRenderDrawColor(rend, 60, 60, 60, 255);
		SDL_RenderLines(rend, axis, 2);

		// origin cross at x=0: segmented lines, skip segments behind camera
		SDL_SetRenderDrawColor(rend, 80, 80, 80, 255);
		float tick = 10.0f;
		int segs = 40;
		vec3 cross_lines[2][2] = {{{0,-tick,0},{0,tick,0}}, {{0,0,-tick},{0,0,tick}}};
		for(auto &line : cross_lines) {
			for(int i = 0; i < segs; i++) {
				double t0 = (double)i / segs;
				double t1 = (double)(i + 1) / segs;
				vec3 p0 = line[0] + (line[1] - line[0]) * t0;
				vec3 p1 = line[0] + (line[1] - line[0]) * t1;
				if(vp.transform_w(p0) < 0.01 || vp.transform_w(p1) < 0.01) continue;
				SDL_FPoint sp[2] = {
					project_to_screen(vp.transform(p0), r.x, r.y, r.w, r.h),
					project_to_screen(vp.transform(p1), r.x, r.y, r.w, r.h),
				};
				SDL_RenderLines(rend, sp, 2);
			}
		}
	}

	void draw_potentials(SDL_Renderer *rend, const Simulation &sim, int n,
	                     const mat4 &vp, SDL_Rect &r) {
		auto *pot = sim.potential;
		SDL_SetRenderDrawColor(rend, 120, 120, 120, 200);
		int start = -1;
		for(int i = 0; i <= n; i++) {
			double v = 0;
			if(i < n) {
				int coords[MAX_RANK]{};
				for(int d = 0; d < sim.grid.rank; d++)
					coords[d] = m_slice_pos[d];
				coords[m_slice_axis] = i;
				size_t idx = sim.grid.linear_index(coords);
				v = pot[idx].real();
			}
			if(v > 0 && start < 0) {
				start = i;
			} else if(v <= 0 && start >= 0) {
				double x0 = -1.0 + 2.0 * start / (n - 1);
				double x1 = -1.0 + 2.0 * i / (n - 1);
				vec3 p0 = vp.transform({x0, 0, 0});
				vec3 p1 = vp.transform({x1, 0, 0});
				SDL_FPoint s0 = project_to_screen(p0, r.x, r.y, r.w, r.h);
				SDL_FPoint s1 = project_to_screen(p1, r.x, r.y, r.w, r.h);
				float dx = s1.x - s0.x, dy = s1.y - s0.y;
				float len = sqrtf(dx * dx + dy * dy);
				if(len > 0) {
					float nx = -dy / len * 2.5f, ny = dx / len * 2.5f;
					SDL_FColor col = {0.47f, 0.47f, 0.47f, 0.8f};
					SDL_Vertex verts[6] = {
						{{s0.x + nx, s0.y + ny}, col, {0,0}},
						{{s1.x + nx, s1.y + ny}, col, {0,0}},
						{{s1.x - nx, s1.y - ny}, col, {0,0}},
						{{s0.x + nx, s0.y + ny}, col, {0,0}},
						{{s1.x - nx, s1.y - ny}, col, {0,0}},
						{{s0.x - nx, s0.y - ny}, col, {0,0}},
					};
					SDL_RenderGeometry(rend, nullptr, verts, 6, nullptr, 0);
				}
				start = -1;
			}
		}
	}

	void draw_stems(SDL_Renderer *rend, int n, const mat4 &vp, SDL_Rect &r,
	                const std::vector<vec3> &pts3d, const std::vector<SDL_FPoint> &helix_pts) {
		int stem_step = (m_stem_density > 0) ? n / m_stem_density : n;
		if(stem_step < 1) stem_step = 1;
		SDL_SetRenderDrawColor(rend, 80, 80, 120, 255);
		for(int i = 0; i < n; i += stem_step) {
			vec3 base = {pts3d[i].x, 0, 0};
			vec3 ndc_base = vp.transform(base);
			SDL_FPoint stem[2] = {
				project_to_screen(ndc_base, r.x, r.y, r.w, r.h),
				helix_pts[i],
			};
			SDL_RenderLines(rend, stem, 2);
		}
	}

	static void hsv_to_rgb(double h, double s, double v, uint8_t &r, uint8_t &g, uint8_t &b) {
		int i = (int)(h * 6);
		double f = h * 6 - i;
		double p = v * (1 - s);
		double q = v * (1 - f * s);
		double t = v * (1 - (1 - f) * s);
		double rr, gg, bb;
		switch(i % 6) {
			case 0: rr=v; gg=t; bb=p; break;
			case 1: rr=q; gg=v; bb=p; break;
			case 2: rr=p; gg=v; bb=t; break;
			case 3: rr=p; gg=q; bb=v; break;
			case 4: rr=t; gg=p; bb=v; break;
			default: rr=v; gg=p; bb=q; break;
		}
		r = (uint8_t)(rr * 255);
		g = (uint8_t)(gg * 255);
		b = (uint8_t)(bb * 255);
	}

	void draw_helix(SDL_Renderer *rend, const std::complex<double> *psi,
	                double max_amp, int n, const std::vector<SDL_FPoint> &helix_pts) {
		if(!m_helix_color) return;
		for(int i = 0; i < n - 1; i++) {
			double amp = std::abs(psi[i]) / max_amp;
			uint8_t cr, cg, cb;
			if(m_helix_color == 1) {
				cr = cg = cb = (uint8_t)(200 * amp + 40 * (1 - amp));
			} else if(m_helix_color == 2) {
				double phase = atan2(psi[i].imag(), psi[i].real());
				double hue = (phase + M_PI) / (2 * M_PI);
				hsv_to_rgb(hue, 1.0, 1.0, cr, cg, cb);
				cr = (uint8_t)(cr * amp + 40 * (1 - amp));
				cg = (uint8_t)(cg * amp + 40 * (1 - amp));
				cb = (uint8_t)(cb * amp + 40 * (1 - amp));
			} else {
				cr = (uint8_t)(255 * amp + 40 * (1 - amp));
				cg = (uint8_t)(255 * fmin(1.0, amp * 2.0) * amp + 40 * (1 - amp));
				cb = (uint8_t)(255 * fmin(1.0, fmax(0.0, amp * 2.0 - 1.0)) * amp + 20 * (1 - amp));
			}
			SDL_SetRenderDrawColor(rend, cr, cg, cb, 255);
			SDL_RenderLine(rend, helix_pts[i].x, helix_pts[i].y,
			               helix_pts[i+1].x, helix_pts[i+1].y);
		}
	}

	double envelope_value(std::complex<double> psi, double max_amp) {
		switch((Envelope)m_envelope) {
			case Envelope::Amplitude:   return std::abs(psi) / max_amp;
			case Envelope::ProbDensity: return std::norm(psi) / (max_amp * max_amp);
			case Envelope::Real:        return psi.real() / max_amp;
			case Envelope::Imaginary:   return psi.imag() / max_amp;
			default: return 0;
		}
	}

	void draw_envelope(SDL_Renderer *rend, const std::complex<double> *psi,
	                   double max_amp, int n, const mat4 &vp, SDL_Rect &r) {
		if((Envelope)m_envelope == Envelope::Off) return;
		SDL_SetRenderDrawColor(rend, 100, 200, 100, 180);
		for(int i = 0; i < n - 1; i++) {
			double t0 = (double)i / (n - 1);
			double t1 = (double)(i+1) / (n - 1);
			double x0 = -1.0 + 2.0 * t0;
			double x1 = -1.0 + 2.0 * t1;
			double a0 = envelope_value(psi[i], max_amp) * m_amplitude;
			double a1 = envelope_value(psi[i+1], max_amp) * m_amplitude;
			vec3 p0 = vp.transform({x0, a0, 0});
			vec3 p1 = vp.transform({x1, a1, 0});
			SDL_FPoint sp[2] = {
				project_to_screen(p0, r.x, r.y, r.w, r.h),
				project_to_screen(p1, r.x, r.y, r.w, r.h),
			};
			SDL_RenderLines(rend, sp, 2);
		}
	}


	// --- cursor ---

	float m_cursor_val{0};      // normalized [-1,1] position on x-axis
	bool m_cursor_valid{false};

	void draw_cursor(SDL_Renderer *rend, const Simulation &sim,
	                 const mat4 &vp, SDL_Rect &r, int n) {
		ImVec2 mp = ImGui::GetMousePos();
		bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
		               mp.y >= r.y && mp.y < r.y + r.h;

		if(!in_rect) { m_cursor_valid = false; return; }

		// project x-axis endpoints to screen
		SDL_FPoint s_left  = project_to_screen(vp.transform({-1, 0, 0}), r.x, r.y, r.w, r.h);
		SDL_FPoint s_right = project_to_screen(vp.transform({ 1, 0, 0}), r.x, r.y, r.w, r.h);

		// interpolate mouse X between axis endpoints
		float axis_dx = s_right.x - s_left.x;
		if(fabs(axis_dx) < 1.0f) { m_cursor_valid = false; return; }

		float t = (mp.x - s_left.x) / axis_dx;  // 0..1
		m_cursor_val = -1.0f + 2.0f * t;
		if(m_cursor_val < -1.0f) m_cursor_val = -1.0f;
		if(m_cursor_val >  1.0f) m_cursor_val =  1.0f;
		m_cursor_valid = true;

		// draw circle on the axis at cursor position
		vec3 cursor_pos_3d = {(double)m_cursor_val, 0, 0};
		SDL_FPoint sp = project_to_screen(vp.transform(cursor_pos_3d), r.x, r.y, r.w, r.h);

		SDL_SetRenderDrawColor(rend, 200, 60, 60, 255);
		float cr = 4.0f;
		int segs = 16;
		for(int i = 0; i < segs; i++) {
			float a0 = 2.0f * M_PI * i / segs;
			float a1 = 2.0f * M_PI * (i + 1) / segs;
			SDL_RenderLine(rend, sp.x + cr * cosf(a0), sp.y + cr * sinf(a0),
			               sp.x + cr * cosf(a1), sp.y + cr * sinf(a1));
		}
	}


	// --- controls ---

	void draw_controls(const Simulation &sim) {
		static const char *helix_color_names[] = { "off", "gray", "rainbow", "flame" };

		ImGui::Text("Env:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::Combo("##envelope", &m_envelope, envelope_names, (int)Envelope::COUNT);
		ImGui::SameLine();

		ImGui::Text("Helix:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::Combo("##helixcol", &m_helix_color, helix_color_names, 4);
		ImGui::SameLine();

		ImGui::Text("Vectors:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::SliderInt("##stems", &m_stem_density, 0, 512, "%d");
		ImGui::SameLine();

		ImGui::Text("Amp:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::SliderFloat("##amp", &m_amplitude, 0.0f, 1.0f, "%.2f");
		ImGui::SameLine();

		// mode controls
		if(sim.grid.rank > 1) {
			ImGui::Text("Axis:");
			ImGui::SameLine();
			for(int d = 0; d < sim.grid.rank; d++) {
				ImGui::SameLine();
				ImGui::PushID(d);
				char label[8]; snprintf(label, sizeof(label), "%d", d);
				ImGui::RadioButton(label, &m_slice_axis, d);
				ImGui::PopID();
			}
			ImGui::SameLine();
			static const char *mode_names[] = { "Slice", "Marginal", "Momentum" };
			if(ImGui::Button(mode_names[m_slice_mode])) {
				m_slice_mode = (m_slice_mode + 1) % 3;
			}
		} else {
			if(ImGui::Button(m_slice_mode == Momentum ? "Momentum" : "Position")) {
				m_slice_mode = (m_slice_mode == Momentum) ? Slice : Momentum;
			}
		}

		// slice cursor sync
		if(m_slice_mode != Marginal) {
			for(int d = 0; d < sim.grid.rank; d++)
				m_slice_pos[d] = m_view.cursor[d];
			m_view.add_slice(m_slice_axis, m_slice_pos);
		}

		// cursor readout
		if(m_cursor_valid) {
			auto &ax = sim.grid.axes[m_slice_axis];
			if(m_slice_mode == Momentum) {
				double L = ax.max - ax.min;
				double dk = 2.0 * M_PI / L;
				int n = ax.points;
				double k = m_cursor_val * (n / 2) * dk;
				double p = k * 1.054571817e-34; // hbar * k
				ImGui::Text("k=%.2e 1/m  p=%.2e kg·m/s", k, p);
			} else {
				double x = ax.min + (m_cursor_val * 0.5 + 0.5) * (ax.max - ax.min);
				ImGui::Text("x=%.2e m", x);
			}
		}
	}


	// --- input ---

	void handle_mouse(SDL_Rect &r) {
		ImVec2 mp = ImGui::GetMousePos();
		bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
		               mp.y >= r.y && mp.y < r.y + r.h;
		bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

		// MMB: orbit, Shift+MMB: pan
		if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
			if(shift)
				m_panning = true;
			else
				m_orbiting = true;
			m_drag_x = mp.x;
			m_drag_y = mp.y;
		}
		if(m_orbiting && ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !shift) {
			float dx = mp.x - m_drag_x;
			float dy = mp.y - m_drag_y;
			m_yaw   -= dx * 0.005;
			m_pitch += dy * 0.005;
			if(m_pitch >  M_PI * 0.49) m_pitch =  M_PI * 0.49;
			if(m_pitch < -M_PI * 0.49) m_pitch = -M_PI * 0.49;
			m_drag_x = mp.x;
			m_drag_y = mp.y;
		}
		if(m_panning && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
			float dx = mp.x - m_drag_x;
			float dy = mp.y - m_drag_y;
			double scale = m_dist * 0.002;
			m_pan_x -= dx * scale * cos(m_yaw);
			m_pan_y += dy * scale;
			m_drag_x = mp.x;
			m_drag_y = mp.y;
		}
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
			m_orbiting = false;
			m_panning = false;
		}

		// scroll wheel: zoom
		if(in_rect) {
			float wheel = ImGui::GetIO().MouseWheel;
			if(wheel != 0) {
				m_dist *= (1.0 - wheel * 0.1);
				if(m_dist < 0.1) m_dist = 0.1;
				if(m_dist > 50.0) m_dist = 50.0;
			}
		}
	}

	bool key(ImGuiKey numpad, ImGuiKey regular) {
		return ImGui::IsKeyPressed(numpad) || ImGui::IsKeyPressed(regular);
	}

	void handle_keys() {
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
			if(ctrl) { m_yaw = 0; m_pitch = -M_PI*0.49; }
			else     { m_yaw = 0; m_pitch =  M_PI*0.49; }
		}
		if(key(ImGuiKey_Keypad5, ImGuiKey_5)) { m_ortho = !m_ortho; }

		// A: reset everything to defaults
		if(ImGui::IsKeyPressed(ImGuiKey_A)) {
			m_yaw = 0; m_pitch = 0; m_dist = 2.5;
			m_pan_x = 0; m_pan_y = 0;
			m_ortho = true;
			m_amplitude = 1.0f;
			m_envelope = 1;
			m_helix_color = 1;
			m_stem_density = 64;
			m_slice_mode = Slice;
		}

		if(key(ImGuiKey_Keypad4, ImGuiKey_4)) { m_yaw -= M_PI/12; }
		if(key(ImGuiKey_Keypad6, ImGuiKey_6)) { m_yaw += M_PI/12; }
		if(key(ImGuiKey_Keypad8, ImGuiKey_8)) {
			m_pitch += M_PI/12;
			if(m_pitch > M_PI*0.49) m_pitch = M_PI*0.49;
		}
		if(key(ImGuiKey_Keypad2, ImGuiKey_2)) {
			m_pitch -= M_PI/12;
			if(m_pitch < -M_PI*0.49) m_pitch = -M_PI*0.49;
		}
	}
};

REGISTER_WIDGET(WidgetHelix,
	.name = "helix",
	.description = "3D helix wavefunction viewer",
);
