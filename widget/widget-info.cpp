#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"
#include "config.hpp"
#include "misc.hpp"

class WidgetInfo : public Widget {
public:
	WidgetInfo(Widget::Info &info) : Widget(info) {}

	void do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r) override;

	void do_save(ConfigWriter &cfg) override {
		// nothing widget-specific to save yet — timescale/dt live on experiment
	}

	void do_load(ConfigReader::Node *node) override {
	}
};


void WidgetInfo::do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	auto &exp = ctx.experiment();
	SDL_SetRenderDrawColor(rend, 20, 20, 25, 255);
	SDL_RenderFillRect(rend, nullptr);

	bool rev = exp.timescale < 0;

	if(ImGui::BeginTable("controls", 2, ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("ctrl", ImGuiTableColumnFlags_WidthStretch);

		// transport + speed + dt on one line
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if(ImGui::Button(exp.running ? "||" : ">"))
			exp.running = !exp.running;
		ImGui::SameLine();
		if(ImGui::Button(rev ? "<" : ">"))
			exp.timescale = -exp.timescale;
		ImGui::TableNextColumn();

		// speed slider
		float avail = ImGui::GetContentRegionAvail().x;
		float half = (avail - 4) * 0.5f;
		ImGui::SetNextItemWidth(half);
		float log_ts = log10f(fabs(exp.timescale));
		float log_ts_def = log10f(fabs(exp.setup.default_timescale));
		char ts_label[64];
		snprintf(ts_label, sizeof(ts_label), "speed: ");
		humanize_unit(fabs(exp.timescale), "s/s", ts_label + 7, sizeof(ts_label) - 7);
		if(ImGui::SliderFloat("##speed", &log_ts, log_ts_def - 4, log_ts_def + 4, ts_label))
			exp.timescale = (rev ? -1.0 : 1.0) * pow(10.0, log_ts);

		if(!exp.simulations.empty()) {
			auto &sim = *exp.simulations[0];

			// dt slider
			ImGui::SameLine();
			ImGui::SetNextItemWidth(half);
			float log_dt = log10f(fabs(sim.dt));
			float log_dt_def = (exp.setup.default_dt > 0) ? log10f(exp.setup.default_dt) : log_dt;
			char dt_label[64];
			snprintf(dt_label, sizeof(dt_label), "dt: ");
			humanize_unit(fabs(sim.dt), "s", dt_label + 4, sizeof(dt_label) - 4);
			if(ImGui::SliderFloat("##dt", &log_dt, log_dt_def - 3, log_dt_def + 3, dt_label)) {
				double new_dt = (sim.dt < 0 ? -1.0 : 1.0) * pow(10.0, log_dt);
				for(auto &s : exp.simulations)
					s->set_dt(new_dt);
			}

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

	// experiment title and description
	if(!exp.setup.title.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", exp.setup.title.c_str());
		if(!exp.setup.description.empty())
			ImGui::TextWrapped("%s", exp.setup.description.c_str());
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
				i, types[(int)pot.type], pot.height / 1.602e-19);
		}
	}

	// A: reset speed and dt to auto-computed defaults
	if(ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_A)) {
		exp.timescale = exp.setup.default_timescale;
		if(exp.setup.default_dt > 0)
			for(auto &sim : exp.simulations)
				sim->set_dt(exp.timescale >= 0 ? exp.setup.default_dt : -exp.setup.default_dt);
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
