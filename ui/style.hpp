
#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include "config.hpp"

class Style {

public:

	struct Color {
		float r, g, b, a;
		operator ImVec4() const { return ImVec4(r, g, b, a); }
		operator SDL_FColor() const { return SDL_FColor{ r, g, b, a }; }
		operator SDL_Color() const { return SDL_Color{ uint8_t(r * 255), uint8_t(g * 255), uint8_t(b * 255), uint8_t(a * 255) }; }
		operator ImU32() const { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)); }
	};

	enum ColorId {
		// UI chrome
		Background,
		Cursor,
		Grid1,
		PanelBorder,
		ToggleButtonOff,
		ToggleButtonOn,
		// GL backgrounds
		BgGl,
		BgGrid,
		// cursors
		CursorEdge,
		CursorCross,
		// borders/grid
		GridBorder,
		Absorb,
		Gridline0,
		Gridline1,
		Gridline2,
		// helix defaults
		HelixDefault,
		SurfaceDefault,
		EnvelopeDefault,
		// potential
		PotentialMarginal,
		COUNT,
	};

	enum Mode { Normal, Presentation };

	static void load(ConfigReader::Node *node);
	static void save(ConfigWriter &cfg);

	static Color color(int cid);
	static void set_color(int cid, const Color &color);

	static Mode mode();
	static void set_mode(Mode m);
	static void toggle_mode();

	static float line_width();
	static float font_scale();

};

		
int SDL_SetRenderDrawColor(SDL_Renderer *rend, Style::Color col);

