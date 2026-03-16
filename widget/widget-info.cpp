#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "experiment.hpp"
#include "config.hpp"

class WidgetInfo : public Widget {
public:
	WidgetInfo(Widget::Info &info) : Widget(info) {}

	void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override;

	void do_save(ConfigWriter &cfg) override {
		// nothing widget-specific to save yet — timescale/dt live on experiment
	}

	void do_load(ConfigReader::Node *node) override {
	}
};


void WidgetInfo::do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r)
{
	SDL_SetRenderDrawColor(rend, 20, 20, 25, 255);
	SDL_RenderFillRect(rend, nullptr);

	ImVec4 col_ok   = ImVec4(0.4, 0.8, 0.4, 1);
	ImVec4 col_warn = ImVec4(0.9, 0.8, 0.2, 1);
	ImVec4 col_bad  = ImVec4(1.0, 0.3, 0.3, 1);
	bool rev = exp.timescale < 0;

	if(ImGui::BeginTable("controls", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("ctrl", ImGuiTableColumnFlags_WidthStretch);

		// transport
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if(ImGui::Button(exp.running ? "||" : ">"))
			exp.running = !exp.running;
		ImGui::SameLine();
		if(ImGui::Button(rev ? "<" : ">"))
			exp.timescale = -exp.timescale;
		ImGui::TableNextColumn();
		ImGui::Text("t = %.4e s", exp.sim_time);

		// speed
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Time");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		float log_ts = log10f(fabs(exp.timescale));
		if(ImGui::SliderFloat("##speed", &log_ts, -14.0f, -5.0f, "speed: 1e%.1f x"))
			exp.timescale = (rev ? -1.0 : 1.0) * pow(10.0, log_ts);

		if(!exp.simulations.empty()) {
			auto &sim = *exp.simulations[0];

			// dt
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1);
			float log_dt = log10f(fabs(sim.dt));
			if(ImGui::SliderFloat("##dt", &log_dt, -14.0f, -8.0f, "step: 1e%.1f s")) {
				double new_dt = (sim.dt < 0 ? -1.0 : 1.0) * pow(10.0, log_dt);
				for(auto &s : exp.simulations)
					s->set_dt(new_dt);
			}

			// phases
			double pp = sim.max_potential_phase;
			double kp = sim.max_kinetic_phase;
			ImVec4 col_p = (pp < 0.3) ? col_ok : (pp < 1.0) ? col_warn : col_bad;
			ImVec4 col_k = (kp < 0.3) ? col_ok : (kp < 1.0) ? col_warn : col_bad;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Phase");
			ImGui::TableNextColumn();
			ImGui::TextColored(col_p, "V %.2f", pp);
			ImGui::SameLine();
			ImGui::TextColored(col_k, "K %.2f", kp);

			// spatial aliasing: k / k_nyquist per axis
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Grid");
			ImGui::TableNextColumn();
			for(int d = 0; d < sim.grid.rank; d++) {
				double kr = sim.k_nyquist_ratio[d];
				ImVec4 col_kr = (kr < 0.3) ? col_ok : (kr < 0.5) ? col_warn : col_bad;
				if(d > 0) ImGui::SameLine();
				ImGui::TextColored(col_kr, "%d:%.0f%%", d, kr * 100);
			}

			// probability + absorb
			double prob = sim.total_probability();
			ImVec4 col_prob = (fabs(prob - 1.0) < 0.01) ? col_ok :
			                  (fabs(prob - 1.0) < 0.05) ? col_warn : col_bad;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("P");
			ImGui::TableNextColumn();
			ImGui::TextColored(col_prob, "%.6f", prob);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			bool ab = sim.absorbing_boundary;
			ImGui::Text("Absorb");
			ImGui::SameLine();
			if(ImGui::Checkbox("##Absorb", &ab)) {
				for(auto &s : exp.simulations)
					s->set_absorbing_boundary(ab);
			}

			if(sim.absorbing_boundary) {
				// width
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1);
				float w = (float)sim.absorb_width;
				if(ImGui::SliderFloat("##abw", &w, 0.01f, 0.3f, "width: %.2f")) {
					for(auto &s : exp.simulations) {
						s->absorb_width = w;
						s->recompute_boundary();
					}
				}

				// strength (log scale)
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1);
				float log_s = log10f(sim.absorb_strength);
				if(ImGui::SliderFloat("##abs", &log_s, -25.0f, -20.0f, "depth: 1e%.2f J")) {
					for(auto &s : exp.simulations) {
						s->absorb_strength = pow(10.0, log_s);
						s->recompute_boundary();
					}
				}
			}
		}

		ImGui::EndTable();
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


REGISTER_WIDGET(WidgetInfo,
	.name = "info",
	.description = "Experiment status",
	.hotkey = ImGuiKey_F1,
)
