
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

static ColorDef colordef_list[Style::COUNT] = {
	DEF_COLOR(Background,        0.00, 0.00, 0.00, 1.00),
	DEF_COLOR(Cursor,            1.00, 1.00, 0.75, 1.00),
	DEF_COLOR(Grid1,             0.25, 0.25, 0.35, 1.00),
	DEF_COLOR(PanelBorder,       0.00, 0.50, 0.50, 1.00),
	DEF_COLOR(ToggleButtonOff,   0.26, 0.26, 0.38, 1.00),
	DEF_COLOR(ToggleButtonOn,    0.26, 0.59, 0.98, 1.00),
};


void Style::load(ConfigReader::Node *node)
{
	for(auto &cd : colordef_list) {
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
	for(auto &cd : colordef_list) {
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
	return colordef_list[cid].color;
}


void Style::set_color(int cid, const Color &color)
{
	assert(cid >= 0 && cid < Style::COUNT);
	colordef_list[cid].color = color;
}


int SDL_SetRenderDrawColor(SDL_Renderer *rend, Style::Color col)
{
	SDL_Color c = col;
	return SDL_SetRenderDrawColor(rend, c.r, c.g, c.b, c.a);
}	
