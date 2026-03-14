#include <SDL3/SDL.h>
#include <imgui.h>

#include "misc.hpp"
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "style.hpp"


Widget::Widget(Widget::Info &info)
	: m_info(info)
{
}


Widget::~Widget()
{
}


void Widget::load(ConfigReader::Node *node)
{
	m_view.load(node);
	do_load(node->find("config"));
}


void Widget::save(ConfigWriter &cw)
{
	cw.write("widget", m_info.name);
	m_view.save(cw);
	cw.push("config");
	do_save(cw);
	cw.pop();
}


Widget *Widget::copy()
{
	Widget *w_new = Widgets::create_widget(m_info.name);
	do_copy(w_new);
	copy_to(w_new);
	return w_new;
}


void Widget::copy_to(Widget *w)
{
	w->m_view = m_view;
}


void Widget::draw(View &view, Experiment &exp, SDL_Renderer *rend, SDL_Rect &r)
{
	if(m_view.lock) m_view = view;

	double t1 = hirestime();
	do_draw(exp, rend, r);
	double t2 = hirestime();

	// draw render time
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 60, ImGui::GetWindowHeight() - 25));
	ImGui::Text("%.2f ms", (t2 - t1) * 1000.0f);

	if(m_view.lock) view = m_view;
}
