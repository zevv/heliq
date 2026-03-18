#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "widget.hpp"
#include "view.hpp"
#include "config.hpp"

class SimContext;

class Panel {

public:

	enum class Type {
		Root, None, Widget, SplitH, SplitV
	};

	Panel(Widget *widget);
	Panel(Type type);
	~Panel();

	void save(ConfigWriter &cfg);
	void load(ConfigReader::Node *node);

	void add(Panel *p, Panel *p_after = nullptr);
	void add(Widget *widget);
	void replace(Panel *kid_old, Panel *kid_new);
	void remove(Panel *kid);
	void update_kid(Panel *pk, int dx, int dy, int dw, int dh);
	void draw(View &view, SimContext &ctx, SDL_Renderer *rend, int x, int y, int w, int h);
	void dump(int depth = 0);
	int nkids() { return (int)m_kids.size(); }

	Type type() { return m_type; }

private:

	struct AddRequest {
		Panel *p_new;
		Panel *p_after;
	};

	Panel *m_parent{};
	Widget *m_widget{};
	int m_last_w{};
	int m_last_h{};
	const char *m_title{};
	Type m_type{};
	float m_weight{1.0};
	std::vector<Panel *> m_kids{};
	std::vector<Panel *> m_kids_remove{};
	std::vector<AddRequest> m_add_requests{};
	bool m_has_focus{};
	float m_background_alpha{0};
};
