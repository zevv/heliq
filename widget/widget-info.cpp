#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"

class WidgetInfo : public Widget {
public:
	WidgetInfo(Widget::Info &info) : Widget(info) {}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
		SDL_SetRenderDrawColor(rend, 20, 20, 25, 255);
		SDL_RenderFillRect(rend, nullptr);

		// transport controls at top
		if(ImGui::Button(exp.running ? "Pause" : "Play"))
			exp.running = !exp.running;
		ImGui::SameLine();
		ImGui::Text("t = %.4e s", exp.sim_time);

		ImGui::Text("Speed:");
		ImGui::SameLine();
		bool rev = exp.timescale < 0;
		if(ImGui::Button(rev ? "<" : ">")) exp.timescale = -exp.timescale;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		float log_ts = log10f(fabs(exp.timescale));
		if(ImGui::SliderFloat("##speed", &log_ts, -18.0f, -9.0f, "1e%.1f s/s"))
			exp.timescale = (rev ? -1.0 : 1.0) * pow(10.0, log_ts);

		if(!exp.simulations.empty()) {
			auto &sim = *exp.simulations[0];

			ImGui::Text("dt:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-1);
			float log_dt = log10f(sim.dt);
			if(ImGui::SliderFloat("##dt", &log_dt, -18.0f, -11.0f, "1e%.1f s")) {
				double new_dt = pow(10.0, log_dt);
				for(auto &s : exp.simulations)
					s->set_dt(new_dt);
			}

			// phase diagnostics
			double pp = sim.max_potential_phase;
			double kp = sim.max_kinetic_phase;
			ImVec4 col_ok = ImVec4(0.4, 0.8, 0.4, 1);
			ImVec4 col_warn = ImVec4(0.9, 0.8, 0.2, 1);
			ImVec4 col_bad = ImVec4(1.0, 0.3, 0.3, 1);
			ImVec4 col_p = (pp < 0.3) ? col_ok : (pp < 1.0) ? col_warn : col_bad;
			ImVec4 col_k = (kp < 0.3) ? col_ok : (kp < 1.0) ? col_warn : col_bad;
			ImGui::TextColored(col_p, "V phase: %.2f rad", pp);
			ImGui::SameLine();
			ImGui::TextColored(col_k, "K phase: %.2f rad", kp);

			// probability conservation
			double prob = sim.total_probability();
			ImVec4 col_prob = (fabs(prob - 1.0) < 0.01) ? col_ok :
			                  (fabs(prob - 1.0) < 0.05) ? col_warn : col_bad;
			ImGui::TextColored(col_prob, "P(total) = %.6f", prob);
		}

		// experiment info below
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto &s = exp.setup;
		ImGui::Text("Experiment  (%dD)", s.spatial_dims);

		// domain
		for(int i = 0; i < s.spatial_dims; i++) {
			auto &ax = s.domain[i];
			ImGui::Text("  axis %d: [%.2g .. %.2g] m  %d pts  dx=%.2g m",
				i, ax.min, ax.max, ax.points, ax.dx());
		}

		// particles
		if(!s.particles.empty()) {
			ImGui::Spacing();
			ImGui::Text("Particles: %zu", s.particles.size());
			for(size_t i = 0; i < s.particles.size(); i++) {
				auto &p = s.particles[i];
				ImGui::Text("  %zu: mass=%.3e kg", i, p.mass);
				ImGui::Text("     pos=[%.2e]  mom=[%.2e]  w=%.2e",
					p.position[0], p.momentum[0], p.width);
			}
		}

		// potentials
		if(!s.potentials.empty()) {
			ImGui::Spacing();
			ImGui::Text("Potentials: %zu", s.potentials.size());
			for(size_t i = 0; i < s.potentials.size(); i++) {
				auto &pot = s.potentials[i];
				const char *types[] = {"barrier", "well", "harmonic", "absorbing"};
				ImGui::Text("  %zu: %s  height=%.2g eV",
					i, types[pot.type], pot.height / 1.602e-19);
			}
		}

		// simulations
		if(!s.simulations.empty()) {
			ImGui::Spacing();
			ImGui::Text("Simulations: %zu", s.simulations.size());
			for(size_t i = 0; i < s.simulations.size(); i++) {
				auto &sim = s.simulations[i];
				ImGui::Text("  %zu: \"%s\"  mode=%s  dt=%.2e s",
					i, sim.name.c_str(),
					sim.mode == SimMode::Joint ? "joint" : "factored",
					sim.dt);
			}
		}
	}

	void do_save(ConfigWriter &cfg) override {
		// nothing widget-specific to save yet — timescale/dt live on experiment
	}

	void do_load(ConfigReader::Node *node) override {
	}
};

REGISTER_WIDGET(WidgetInfo,
	.name = "info",
	.description = "Experiment status",
	.hotkey = ImGuiKey_F2,
)
