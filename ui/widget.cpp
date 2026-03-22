#include <SDL3/SDL.h>
#include <imgui.h>

#include "misc.hpp"
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"
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
	do_load(node->find("config"));
}


void Widget::save(ConfigWriter &cw)
{
	cw.write("widget", m_info.name);
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


void Widget::draw(View &view, SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r)
{
	if(m_view.lock) m_view = view;

	double t1 = hirestime();
	do_draw(ctx, rend, r);
	double t2 = hirestime();

	// draw cursor positions (bottom-left)
	auto &st = ctx.state();
	if(st.grid.rank > 0) {
		char buf[256];
		int pos = 0;
		for(int d = 0; d < st.grid.rank; d++) {
			auto &ax = st.grid.axes[d];
			double val = ax.min + m_view.cursor[d] * ax.dx();
			char vbuf[32];
			humanize_unit(val, "m", vbuf, sizeof(vbuf));
			if(d > 0) buf[pos++] = ' ';
			pos += snprintf(buf + pos, sizeof(buf) - pos, "%s=%s", ax.label, vbuf);
		}
		ImGui::SetCursorPos(ImVec2(4, ImGui::GetWindowHeight() - 25));
		ImGui::TextShadow("%s", buf);
	}

	// draw render time (bottom-right)
	ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 60, ImGui::GetWindowHeight() - 25));
	ImGui::Text("%.2f ms", (t2 - t1) * 1000.0f);

	if(m_view.lock) view = m_view;
}
