#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"

class WidgetInfo : public Widget {
public:
	WidgetInfo(Widget::Info &info) : Widget(info) {}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
		SDL_SetRenderDrawColor(rend, 20, 20, 25, 255);
		SDL_RenderFillRect(rend, nullptr);

		auto &s = exp.setup;

		ImGui::Text("Experiment");
		ImGui::Separator();

		ImGui::Text("dimensions: %d", s.spatial_dims);

		// domain
		ImGui::Spacing();
		ImGui::Text("Domain:");
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

		// transport controls
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if(ImGui::Button(exp.running ? "Pause" : "Play"))
			exp.running = !exp.running;
		ImGui::SameLine();
		ImGui::Text("t = %.4e s", exp.sim_time);

		ImGui::Text("Speed:");
		ImGui::SetNextItemWidth(-1);
		float log_ts = log10f(exp.timescale);
		if(ImGui::SliderFloat("##speed", &log_ts, -18.0f, -9.0f, "1e%.0f s/s"))
			exp.timescale = pow(10.0, log_ts);

		if(!exp.simulations.empty()) {
			ImGui::Text("dt:");
			ImGui::SetNextItemWidth(-1);
			float log_dt = log10f(exp.simulations[0]->dt);
			if(ImGui::SliderFloat("##dt", &log_dt, -18.0f, -13.0f, "1e%.0f s")) {
				double new_dt = pow(10.0, log_dt);
				for(auto &sim : exp.simulations)
					sim->set_dt(new_dt);
			}
		}
	}
};

REGISTER_WIDGET(WidgetInfo,
	.name = "info",
	.description = "Experiment status",
	.hotkey = ImGuiKey_F2,
)
