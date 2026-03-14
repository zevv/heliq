#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <complex>
#include <vector>

#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"
#include "math3d.hpp"
#include "grid.hpp"

enum class Envelope { Amplitude, ProbDensity, Real, Imaginary, COUNT };

static const char *envelope_names[] = { "|psi|", "|psi|^2", "Re(psi)", "Im(psi)" };

class WidgetHelix : public Widget {
public:
	WidgetHelix(Widget::Info &info) : Widget(info) {}

	void do_save(ConfigWriter &cfg) override {
		cfg.write("yaw", m_yaw);
		cfg.write("pitch", m_pitch);
		cfg.write("dist", m_dist);
		cfg.write("pan_x", m_pan_x);
		cfg.write("pan_y", m_pan_y);
		cfg.write("ortho", m_ortho);
		cfg.write("envelope", m_envelope);
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

		// default slice positions to center if not set
		for(int d = 0; d < sim.grid.rank; d++) {
			if(m_slice_pos[d] == 0 && d != m_slice_axis)
				m_slice_pos[d] = sim.grid.axes[d].points / 2;
		}

		// extract a 1D slice from the grid
		// for rank > 1, fix all other axes at m_slice_pos[d]
		m_slice.resize(n);
		if(sim.grid.rank == 1) {
			for(int i = 0; i < n; i++)
				m_slice[i] = psi_all[i];
		} else {
			for(int i = 0; i < n; i++) {
				int coords[MAX_RANK]{};
				for(int d = 0; d < sim.grid.rank; d++)
					coords[d] = m_slice_pos[d];
				coords[m_slice_axis] = i;
				size_t idx = sim.grid.linear_index(coords);
				m_slice[i] = psi_all[idx];
			}
		}
		auto *psi = m_slice.data();

		// find max amplitude across the full grid for consistent scaling
		double max_amp = 1e-30;
		size_t total = sim.grid.total_points();
		for(size_t i = 0; i < total; i++) {
			double a = std::abs(psi_all[i]);
			if(a > max_amp) max_amp = a;
		}

		// handle mouse input for rotation
		handle_mouse(r);

		// build camera
		vec3 center = {m_pan_x, m_pan_y, 0};
		vec3 eye = {
			center.x + m_dist * sin(m_yaw) * cos(m_pitch),
			center.y + m_dist * sin(m_pitch),
			center.z + m_dist * cos(m_yaw) * cos(m_pitch),
		};
		mat4 view = mat4::look_at(eye, center, {0, 1, 0});
		double aspect = (double)r.w / r.h;
		mat4 proj = m_ortho
			? mat4::ortho(m_dist * 0.5, aspect, 0.1, 100.0)
			: mat4::perspective(0.8, aspect, 0.1, 100.0);
		mat4 vp = proj * view;

		// map grid data to 3D points
		// x-axis: position (normalized to [-1, 1])
		// y-axis: Re(ψ) / max_amp
		// z-axis: Im(ψ) / max_amp
		std::vector<SDL_FPoint> helix_pts(n);
		std::vector<vec3> pts3d(n);

		for(int i = 0; i < n; i++) {
			double t = (double)i / (n - 1);
			double x = -1.0 + 2.0 * t;
			double y = psi[i].real() / max_amp;
			double z = psi[i].imag() / max_amp;
			pts3d[i] = {x, y, z};
			vec3 ndc = vp.transform(pts3d[i]);
			helix_pts[i] = project_to_screen(ndc, r.x, r.y, r.w, r.h);
		}

		// draw x-axis
		{
			vec3 a = vp.transform({-1, 0, 0});
			vec3 b = vp.transform({ 1, 0, 0});
			SDL_FPoint axis[2] = {
				project_to_screen(a, r.x, r.y, r.w, r.h),
				project_to_screen(b, r.x, r.y, r.w, r.h),
			};
			SDL_SetRenderDrawColor(rend, 60, 60, 60, 255);
			SDL_RenderLines(rend, axis, 2);
		}

		// draw potential along the slice
		{
			auto *pot = sim.potential;
			SDL_SetRenderDrawColor(rend, 180, 100, 40, 160);
			bool in_barrier = false;
			SDL_FPoint barrier_start{};
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
				double t = (double)i / (n - 1);
				double x = -1.0 + 2.0 * t;
				vec3 ndc = vp.transform({x, 0, 0});
				SDL_FPoint sp = project_to_screen(ndc, r.x, r.y, r.w, r.h);
				if(v > 0 && !in_barrier) {
					barrier_start = sp;
					in_barrier = true;
				} else if((v <= 0 || i == n) && in_barrier) {
					// draw a thick-ish bar for the barrier
					SDL_FRect br = {
						barrier_start.x,
						barrier_start.y - 3,
						sp.x - barrier_start.x,
						6
					};
					SDL_RenderFillRect(rend, &br);
					in_barrier = false;
				}
			}
		}

		// draw stems every N points
		int stem_step = n / 64;
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

		// draw helix line
		// color by phase: map arg(ψ) to hue
		for(int i = 0; i < n - 1; i++) {
			double phase = atan2(psi[i].imag(), psi[i].real());
			double hue = (phase + M_PI) / (2 * M_PI);  // 0..1
			uint8_t cr, cg, cb;
			hsv_to_rgb(hue, 1.0, 1.0, cr, cg, cb);

			// fade by amplitude
			double amp = std::abs(psi[i]) / max_amp;
			cr = (uint8_t)(cr * amp + 40 * (1 - amp));
			cg = (uint8_t)(cg * amp + 40 * (1 - amp));
			cb = (uint8_t)(cb * amp + 40 * (1 - amp));

			SDL_SetRenderDrawColor(rend, cr, cg, cb, 255);
			SDL_RenderLine(rend, helix_pts[i].x, helix_pts[i].y,
			                     helix_pts[i+1].x, helix_pts[i+1].y);
		}

		// draw envelope curve on the x-axis plane
		{
			SDL_SetRenderDrawColor(rend, 100, 200, 100, 180);
			for(int i = 0; i < n - 1; i++) {
				double t0 = (double)i / (n - 1);
				double t1 = (double)(i+1) / (n - 1);
				double x0 = -1.0 + 2.0 * t0;
				double x1 = -1.0 + 2.0 * t1;
				double a0 = envelope_value(psi[i], max_amp);
				double a1 = envelope_value(psi[i+1], max_amp);
				vec3 p0 = vp.transform({x0, a0, 0});
				vec3 p1 = vp.transform({x1, a1, 0});
				SDL_FPoint sp[2] = {
					project_to_screen(p0, r.x, r.y, r.w, r.h),
					project_to_screen(p1, r.x, r.y, r.w, r.h),
				};
				SDL_RenderLines(rend, sp, 2);
			}
		}

		// view presets (Blender numpad style)
		handle_keys();

		// controls
		ImGui::SetNextItemWidth(120);
		ImGui::Combo("##envelope", &m_envelope, envelope_names, (int)Envelope::COUNT);
		ImGui::SameLine();
		ImGui::Text("%s  yaw=%.0f  pitch=%.0f", m_ortho ? "ortho" : "persp",
			m_yaw * 180/M_PI, m_pitch * 180/M_PI);

		// slice controls for rank > 1
		if(sim.grid.rank > 1) {
			ImGui::SetNextItemWidth(80);
			ImGui::SliderInt("##axis", &m_slice_axis, 0, sim.grid.rank - 1, "axis %d");
			for(int d = 0; d < sim.grid.rank; d++) {
				if(d == m_slice_axis) continue;
				ImGui::SameLine();
				ImGui::PushID(d);
				ImGui::SetNextItemWidth(80);
				ImGui::SliderInt("##sl", &m_slice_pos[d], 0, sim.grid.axes[d].points - 1);
				ImGui::PopID();
			}
		}

		// publish slice state to view for other widgets to read
		m_view.add_slice(m_slice_axis, m_slice_pos);
	}

private:
	double m_yaw{0};
	double m_pitch{0};
	double m_dist{2.5};
	double m_pan_x{0}, m_pan_y{0};
	bool m_ortho{true};
	int m_envelope{0};  // Envelope enum
	int m_slice_axis{0};
	int m_slice_pos[MAX_RANK]{};  // fixed position on non-display axes
	std::vector<std::complex<double>> m_slice;
	bool m_orbiting{false};
	bool m_panning{false};
	float m_drag_x{}, m_drag_y{};

	double envelope_value(std::complex<double> psi, double max_amp) {
		switch((Envelope)m_envelope) {
			case Envelope::Amplitude:   return std::abs(psi) / max_amp;
			case Envelope::ProbDensity: return std::norm(psi) / (max_amp * max_amp);
			case Envelope::Real:        return psi.real() / max_amp * 0.5 + 0.5;
			case Envelope::Imaginary:   return psi.imag() / max_amp * 0.5 + 0.5;
			default: return 0;
		}
	}

	bool key(ImGuiKey numpad, ImGuiKey regular) {
		return ImGui::IsKeyPressed(numpad) || ImGui::IsKeyPressed(regular);
	}

	void handle_keys() {
		bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

		if(key(ImGuiKey_Keypad1, ImGuiKey_1)) {
			if(ctrl) { m_yaw = M_PI; m_pitch = 0; }      // back
			else     { m_yaw = 0;    m_pitch = 0; }       // front
		}
		if(key(ImGuiKey_Keypad3, ImGuiKey_3)) {
			if(ctrl) { m_yaw = -M_PI/2; m_pitch = 0; }   // left
			else     { m_yaw =  M_PI/2; m_pitch = 0; }   // right
		}
		if(key(ImGuiKey_Keypad7, ImGuiKey_7)) {
			if(ctrl) { m_yaw = 0; m_pitch = -M_PI*0.49; } // bottom
			else     { m_yaw = 0; m_pitch =  M_PI*0.49; } // top
		}
		if(key(ImGuiKey_Keypad5, ImGuiKey_5)) { m_ortho = !m_ortho; }

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
			m_yaw   += dx * 0.005;
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

	void hsv_to_rgb(double h, double s, double v, uint8_t &r, uint8_t &g, uint8_t &b) {
		int hi = (int)(h * 6.0) % 6;
		double f = h * 6.0 - hi;
		double p = v * (1 - s);
		double q = v * (1 - f * s);
		double t = v * (1 - (1 - f) * s);
		double rr, gg, bb;
		switch(hi) {
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
};

REGISTER_WIDGET(WidgetHelix,
	.name = "helix",
	.description = "3D wavefunction helix",
	.hotkey = ImGuiKey_F4,
)
