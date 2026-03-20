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

	void do_save(ConfigWriter &cfg) override {}
	void do_load(ConfigReader::Node *node) override {}
};


void WidgetInfo::do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	auto &st = ctx.state();

	SDL_SetRenderDrawColor(rend, 20, 20, 25, 255);
	SDL_RenderFillRect(rend, nullptr);

	if(st.grid.rank == 0) { ImGui::Text("No simulation"); return; }

	// absorbing boundary
	{
		bool ab = st.absorbing_boundary;
		ImGui::Text("Absorb");
		ImGui::SameLine();
		if(ImGui::Checkbox("##Absorb", &ab))
			ctx.push(CmdSetAbsorb{ab, (float)st.absorb_width, (float)st.absorb_strength});

		if(st.absorbing_boundary) {
			ImGui::SetNextItemWidth(-1);
			float w = (float)st.absorb_width;
			if(ImGui::SliderFloat("##abw", &w, 0.01f, 0.3f, "width: %.2f"))
				ctx.push(CmdSetAbsorb{true, w, (float)st.absorb_strength});

			ImGui::SetNextItemWidth(-1);
			float log_s = log10f(st.absorb_strength);
			if(ImGui::SliderFloat("##abs", &log_s, -25.0f, -20.0f, "depth: 1e%.2f J"))
				ctx.push(CmdSetAbsorb{true, (float)st.absorb_width, (float)pow(10.0, log_s)});
		}
	}

	// experiment title and description
	if(!st.setup.title.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", st.setup.title.c_str());
		if(!st.setup.description.empty())
			ImGui::TextWrapped("%s", st.setup.description.c_str());
	}

	// experiment info below
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	auto &s = st.setup;
	ImGui::Text("Experiment  (%dD)", s.spatial_dims);

	for(int i = 0; i < s.spatial_dims; i++) {
		auto &ax = s.domain[i];
		ImGui::Text("  axis %d: [%.2g .. %.2g] m  %d pts  dx=%.2g m",
			i, ax.min, ax.max, ax.points, ax.dx());
	}

	if(!s.particles.empty()) {
		ImGui::Spacing();
		ImGui::Text("Particles: %zu", s.particles.size());
		for(size_t i = 0; i < s.particles.size(); i++) {
			auto &p = s.particles[i];
			ImGui::Text("  %zu: mass=%.3e kg", i, p.mass);
			ImGui::Text("     pos=[%.2e]  mom=[%.2e]  w=%.2e",
				p.position[0], p.momentum[0], p.width[0]);
		}
	}

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
