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

enum class Envelope { Off, Amplitude, ProbDensity, Real, Imaginary, COUNT };

static const char *envelope_names[] = { "off", "|psi|", "|psi|^2", "Re(psi)", "Im(psi)" };

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
		cfg.write("helix_color", m_helix_color);
		cfg.write("amplitude", m_amplitude);
		cfg.write("stem_density", m_stem_density);
		cfg.write("slice_axis", m_slice_axis);
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
			double y = psi[i].real() / max_amp * m_amplitude;
			double z = psi[i].imag() / max_amp * m_amplitude;
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

		// draw potential regions as 3D quads on the axis
		{
			auto *pot = sim.potential;
			SDL_SetRenderDrawColor(rend, 80, 80, 80, 100);
			double h = 10.0;  // large enough to fill the view
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
					// draw quad from start to i
					double x0 = -1.0 + 2.0 * start / (n - 1);
					double x1 = -1.0 + 2.0 * i / (n - 1);
					vec3 corners[4] = {
						vp.transform({x0, -h, 0}),
						vp.transform({x1, -h, 0}),
						vp.transform({x1,  h, 0}),
						vp.transform({x0,  h, 0}),
					};
					SDL_FPoint sp[4];
					for(int c = 0; c < 4; c++)
						sp[c] = project_to_screen(corners[c], r.x, r.y, r.w, r.h);
					// filled quad as two triangles
					SDL_FColor col = {0.31f, 0.31f, 0.31f, 0.4f};
					SDL_Vertex verts[6] = {
						{{sp[0].x, sp[0].y}, col, {0,0}},
						{{sp[1].x, sp[1].y}, col, {0,0}},
						{{sp[2].x, sp[2].y}, col, {0,0}},
						{{sp[0].x, sp[0].y}, col, {0,0}},
						{{sp[2].x, sp[2].y}, col, {0,0}},
						{{sp[3].x, sp[3].y}, col, {0,0}},
					};
					SDL_RenderGeometry(rend, nullptr, verts, 6, nullptr, 0);
					start = -1;
				}
			}
		}

		// draw stems every N points
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

		// draw helix line
		for(int i = 0; i < n - 1; i++) {
			double amp = std::abs(psi[i]) / max_amp;
			uint8_t cr, cg, cb;
			if(m_helix_color == 0) {
				// gray: white fading by amplitude
				cr = cg = cb = (uint8_t)(200 * amp + 40 * (1 - amp));
			} else if(m_helix_color == 1) {
				// phase: hue from phase angle
				double phase = atan2(psi[i].imag(), psi[i].real());
				double hue = (phase + M_PI) / (2 * M_PI);
				hsv_to_rgb(hue, 1.0, 1.0, cr, cg, cb);
				cr = (uint8_t)(cr * amp + 40 * (1 - amp));
				cg = (uint8_t)(cg * amp + 40 * (1 - amp));
				cb = (uint8_t)(cb * amp + 40 * (1 - amp));
			} else {
				// flame: red -> yellow -> white by amplitude
				cr = (uint8_t)(255 * amp + 40 * (1 - amp));
				cg = (uint8_t)(255 * fmin(1.0, amp * 2.0) * amp + 40 * (1 - amp));
				cb = (uint8_t)(255 * fmin(1.0, fmax(0.0, amp * 2.0 - 1.0)) * amp + 20 * (1 - amp));
			}
			SDL_SetRenderDrawColor(rend, cr, cg, cb, 255);
			SDL_RenderLine(rend, helix_pts[i].x, helix_pts[i].y,
			                     helix_pts[i+1].x, helix_pts[i+1].y);
		}

		// draw envelope curve on the x-axis plane
		if((Envelope)m_envelope != Envelope::Off) {
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

		// view presets (Blender numpad style) — only when panel is focused
		if(ImGui::IsWindowFocused()) handle_keys();

		// controls
		static const char *helix_color_names[] = { "gray", "rainbow", "flame" };
		ImGui::SetNextItemWidth(80);
		ImGui::Combo("##envelope", &m_envelope, envelope_names, (int)Envelope::COUNT);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::Combo("##helixcol", &m_helix_color, helix_color_names, 3);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::SliderFloat("##amp", &m_amplitude, 0.0f, 1.0f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::SliderInt("##stems", &m_stem_density, 0, 256, "%d");
		ImGui::SameLine();
		ImGui::Text("%s", m_ortho ? "O" : "P");

		// slice controls for rank > 1
		if(sim.grid.rank > 1) {
			for(int d = 0; d < sim.grid.rank; d++) {
				ImGui::SameLine();
				ImGui::PushID(d);
				char label[8]; snprintf(label, sizeof(label), "%d", d);
				ImGui::RadioButton(label, &m_slice_axis, d);
				ImGui::PopID();
			}
			for(int d = 0; d < sim.grid.rank; d++) {
				if(d == m_slice_axis) continue;
				ImGui::SameLine();
				ImGui::PushID(d + 100);
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
	int m_envelope{1};  // Envelope::Amplitude
	int m_helix_color{1};  // 0=mono, 1=phase
	float m_amplitude{1.0f};  // helix amplitude scale
	int m_stem_density{64};   // stems across the full axis
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
			case Envelope::Real:        return psi.real() / max_amp;
			case Envelope::Imaginary:   return psi.imag() / max_amp;
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
			m_pan_x = 0; m_pan_y = 0; m_dist = 2.5;
		}
		if(key(ImGuiKey_Keypad3, ImGuiKey_3)) {
			if(ctrl) { m_yaw = -M_PI/2; m_pitch = 0; }   // left
			else     { m_yaw =  M_PI/2; m_pitch = 0; }   // right
			m_pan_x = 0; m_pan_y = 0; m_dist = 2.5;
		}
		if(key(ImGuiKey_Keypad7, ImGuiKey_7)) {
			if(ctrl) { m_yaw = 0; m_pitch = -M_PI*0.49; } // bottom
			else     { m_yaw = 0; m_pitch =  M_PI*0.49; } // top
			m_pan_x = 0; m_pan_y = 0; m_dist = 2.5;
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
