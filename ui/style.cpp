
#include <imgui.h>
#include <SDL3/SDL.h>
#include <assert.h>

#include "style.hpp"

struct ColorDef {
	const char *name;
	Style::Color color;

};

#define DEF_COLOR(id, r, g, b, a) \
	[Style::ColorId::id] = { #id, Style::Color{ r, g, b, a } }

static Style::Mode s_mode = Style::Normal;

// Normal palette — same values as current
static ColorDef colordef_normal[Style::COUNT] = {
	DEF_COLOR(Background,        0.00, 0.00, 0.00, 1.00),
	DEF_COLOR(Cursor,            1.00, 1.00, 0.75, 1.00),
	DEF_COLOR(Grid1,             0.25, 0.25, 0.35, 1.00),
	DEF_COLOR(PanelBorder,       0.00, 0.50, 0.50, 1.00),
	DEF_COLOR(ToggleButtonOff,   0.26, 0.26, 0.38, 1.00),
	DEF_COLOR(ToggleButtonOn,    0.26, 0.59, 0.98, 1.00),
	DEF_COLOR(BgGl,              0.04, 0.04, 0.06, 1.00),
	DEF_COLOR(BgGrid,            0.04, 0.04, 0.06, 1.00),
	DEF_COLOR(CursorEdge,        0.90, 0.20, 0.20, 0.60),
	DEF_COLOR(CursorCross,       0.90, 0.20, 0.20, 0.20),
	DEF_COLOR(GridBorder,        0.31, 0.31, 0.31, 1.00),
	DEF_COLOR(Absorb,            0.10, 0.10, 0.78, 1.00),
	DEF_COLOR(Gridline0,         0.18, 0.18, 0.18, 1.00),
	DEF_COLOR(Gridline1,         0.24, 0.24, 0.24, 1.00),
	DEF_COLOR(Gridline2,         0.31, 0.31, 0.31, 1.00),
	DEF_COLOR(HelixDefault,      0.78, 0.78, 0.78, 1.00),
	DEF_COLOR(SurfaceDefault,    0.30, 0.40, 0.70, 1.00),
	DEF_COLOR(EnvelopeDefault,   0.39, 0.78, 0.39, 1.00),
	DEF_COLOR(PotentialMarginal, 0.50, 0.50, 0.50, 1.00),
};

// Presentation palette — high contrast for projectors
static ColorDef colordef_presentation[Style::COUNT] = {
	DEF_COLOR(Background,        0.00, 0.00, 0.00, 1.00),
	DEF_COLOR(Cursor,            1.00, 1.00, 0.80, 1.00),
	DEF_COLOR(Grid1,             0.40, 0.40, 0.55, 1.00),
	DEF_COLOR(PanelBorder,       0.00, 0.70, 0.70, 1.00),
	DEF_COLOR(ToggleButtonOff,   0.35, 0.35, 0.50, 1.00),
	DEF_COLOR(ToggleButtonOn,    0.30, 0.65, 1.00, 1.00),
	DEF_COLOR(BgGl,              0.02, 0.02, 0.04, 1.00),
	DEF_COLOR(BgGrid,            0.02, 0.02, 0.04, 1.00),
	DEF_COLOR(CursorEdge,        1.00, 0.30, 0.30, 0.80),
	DEF_COLOR(CursorCross,       1.00, 0.30, 0.30, 0.35),
	DEF_COLOR(GridBorder,        0.50, 0.50, 0.50, 1.00),
	DEF_COLOR(Absorb,            0.15, 0.15, 0.90, 1.00),
	DEF_COLOR(Gridline0,         0.28, 0.28, 0.28, 1.00),
	DEF_COLOR(Gridline1,         0.38, 0.38, 0.38, 1.00),
	DEF_COLOR(Gridline2,         0.50, 0.50, 0.50, 1.00),
	DEF_COLOR(HelixDefault,      0.90, 0.90, 0.90, 1.00),
	DEF_COLOR(SurfaceDefault,    0.40, 0.55, 0.85, 1.00),
	DEF_COLOR(EnvelopeDefault,   0.50, 0.90, 0.50, 1.00),
	DEF_COLOR(PotentialMarginal, 0.65, 0.65, 0.65, 1.00),
};


void Style::load(ConfigReader::Node *node)
{
	int mode = 0;
	node->read("mode", mode);
	s_mode = mode ? Presentation : Normal;

	for(auto &cd : colordef_normal) {
		const char *hexcolor = node->read_str(cd.name);
		if(hexcolor) {
			unsigned int r, g, b, a;
			if(sscanf(hexcolor, "#%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
				cd.color = Color{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
			}
		}
	}
}


void Style::save(ConfigWriter &cfg)
{
	cfg.write("mode", s_mode == Presentation ? 1 : 0);

	for(auto &cd : colordef_normal) {
		const Color &c = cd.color;
		unsigned int r = static_cast<unsigned int>(c.r * 255);
		unsigned int g = static_cast<unsigned int>(c.g * 255);
		unsigned int b = static_cast<unsigned int>(c.b * 255);
		unsigned int a = static_cast<unsigned int>(c.a * 255);
		char buf[10];
		snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", r, g, b, a);
		cfg.write(cd.name, buf);
	}
}


Style::Color Style::color(int cid)
{
	assert(cid >= 0 && cid < Style::COUNT);
	ColorDef *table = (s_mode == Normal) ? colordef_normal : colordef_presentation;
	return table[cid].color;
}


void Style::set_color(int cid, const Color &color)
{
	assert(cid >= 0 && cid < Style::COUNT);
	colordef_normal[cid].color = color;
}


int SDL_SetRenderDrawColor(SDL_Renderer *rend, Style::Color col)
{
	SDL_Color c = col;
	return SDL_SetRenderDrawColor(rend, c.r, c.g, c.b, c.a);
}


Style::Mode Style::mode()
{
	return s_mode;
}


void Style::set_mode(Mode m)
{
	s_mode = m;
}


void Style::toggle_mode()
{
	s_mode = (s_mode == Normal) ? Presentation : Normal;
}


float Style::line_width()
{
	return (s_mode == Presentation) ? 4.0f : 1.0f;
}


float Style::font_scale()
{
	return (s_mode == Presentation) ? 1.5f : 1.0f;
}

