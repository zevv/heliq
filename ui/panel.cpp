
#include <stdlib.h>
#include <imgui.h>
#include <algorithm>

#include "widgetregistry.hpp"
#include "panel.hpp"
#include "style.hpp"
	

Panel::Panel(Widget *widget)
	: m_widget(widget)
	, m_type(Type::Widget)
{
	char buf[16] = "";
	for(int i=0; i<15; i++) {
		char c = 'a' + (rand() % 26);
		buf[i] = c;
	}
	buf[15] = 0;
	m_title = strdup(buf);
}


Panel::Panel(Type type)
	: m_type(type)
{
	char buf[16] = "";
	for(int i=0; i<15; i++) {
		char c = 'a' + (rand() % 26);
		buf[i] = c;
	}
	buf[15] = 0;
	m_title = strdup(buf);
}


Panel::~Panel()
{
	for(auto &k : m_kids) {
		delete k;
	}
	free((void *)m_title);
	if(m_widget) {
		delete m_widget;
	}
}


void Panel::load(ConfigReader::Node *node)
{
	node->read("weight", m_weight);
	
	if(const char *type = node->read_str("type")) {

		if(strcmp(type, "root") == 0) {
			m_type = Type::Root;
		}

		if(strcmp(type, "split_v") == 0) {
			m_type = Type::SplitV;
		}

		if(strcmp(type, "split_h") == 0) {
			m_type = Type::SplitH;
		}

		if(strcmp(type, "widget") == 0) {
			m_type = Type::Widget;
			if(const char *wtype = node->read_str("widget")) {
				m_widget = Widgets::create_widget(wtype);
				m_widget->load(node);
			}
		}

		for(auto &k : node->kids) {
			if(isdigit(k.first[0])) {
				Panel *p = new Panel(Type::None);
				p->load(k.second);
				m_kids.push_back(p);
				p->m_parent = this;
			}
		}
	}
}


void Panel::save(ConfigWriter &cw)
{
	if(m_type == Type::Root) {
		cw.write("type", "root");
	}
	if(m_type == Type::Widget) {
		cw.write("type", "widget");
		cw.write("weight", m_weight);
		m_widget->save(cw);
	} 
	if(m_type == Type::SplitH) {
		cw.write("type", "split_h");
		cw.write("weight", m_weight);
	} 
	if(m_type == Type::SplitV) {
		cw.write("type", "split_v");
		cw.write("weight", m_weight);
	}
	if(m_kids.size() > 0) {
		for(size_t i=0; i<m_kids.size(); i++) {
			cw.push(i);
			m_kids[i]->save(cw);
			cw.pop();
		}
	}
}

	
void Panel::add(Panel *p, Panel *p_after)
{
	AddRequest req = { p, p_after };
	m_add_requests.push_back(req);
}


void Panel::add(Widget *w)
{
	Panel *p = new Panel(w);
	add(p);
}


void Panel::replace(Panel *kid_old, Panel *kid_new)
{
	for(size_t i=0; i<m_kids.size(); i++) {
		if(m_kids[i] == kid_old) {
			m_kids[i] = kid_new;
			kid_new->m_parent = this;
			kid_new->m_weight = kid_old->m_weight;
			kid_old->m_parent = nullptr;
		}
	}
}


void Panel::remove(Panel *kid)
{
	m_kids_remove.push_back(kid);
}


void Panel::update_kid(Panel *pk, int dx1, int dy1, int dx2, int dy2)
{
	size_t nkids = m_kids.size();

	if(m_type == Type::SplitH) {
		for(size_t i=0; i<m_kids.size(); i++) {
			if(m_kids[i] == pk) {
				if(dx1 != 0 && i > 0) {
					Panel *pp = m_kids[i-1];
					float dw = (float)dx1 / m_last_w;
					pk->m_weight -= dw;
					pp->m_weight += dw;
				}
				if(i < (nkids - 1)) {
					Panel *pn = m_kids[i+1];
					float dw = (float)dx2 / m_last_w;
					pk->m_weight += dw;
					pn->m_weight -= dw;
				}
			}
		}
	}

	if(m_type == Type::SplitV) {
		for(size_t i=0; i<m_kids.size(); i++) {
			if(m_kids[i] == pk) {
				if(dy1 != 0 && i > 0) {
					Panel *pp = m_kids[i-1];
					float dw = (float)dy1 / m_last_h;
					pk->m_weight -= dw;
					pp->m_weight += dw;
				}
				if(i < (nkids - 1)) {
					Panel *pn = m_kids[i+1];
					float dw = (float)dy2 / m_last_h;
					pk->m_weight += dw;
					pn->m_weight -= dw;
				}
			}
		}
	}

	if (m_parent) {
		int my_dx1 = 0;
		int my_dy1 = 0;
		int my_dx2 = 0;
		int my_dy2 = 0;
		if (m_type == Type::SplitH) {
			my_dy1 = dy1;
			my_dy2 = dy2;
			if(pk == m_kids.front()) my_dx1 = dx1;
			if(pk == m_kids.back())  my_dx2 = dx2;
		} else if (m_type == Type::SplitV) {
			my_dx1 = dx1;
			my_dx2 = dx2;
			if(pk == m_kids.front()) my_dy1 = dy1;
			if(pk == m_kids.back())  my_dy2 = dy2;
		} else {
			my_dx1 = dx1; my_dy1 = dy1;
			my_dx2 = dx2; my_dy2 = dy2;
		}
		if (my_dx1 != 0 || my_dy1 != 0 || my_dx2 != 0 || my_dy2 != 0) {
			m_parent->update_kid(this, my_dx1, my_dy1, my_dx2, my_dy2);
		}
	}
}


void Panel::draw(View &view, Experiment &exp, SDL_Renderer *rend, int x, int y, int w, int h)
{
	m_last_w = w;
	m_last_h = h;

	if(m_type == Type::Root) {
		if(m_kids.size() == 1) {
			m_kids[0]->draw(view, exp, rend, x, y, w, h);
		}
	}

	if(m_type == Type::SplitH) {
		int kx = x;
		for(auto &pk : m_kids) {
			int kw = pk->m_weight * (w-1);
			pk->draw(view, exp, rend, kx, y, kw, h);
			kx += kw + 1;
		}

	} 

	if(m_type == Type::SplitV) {
		int ky = y;
		for(auto &pk : m_kids) {
			int kh = pk->m_weight * (h-1);
			pk->draw(view, exp, rend, x, ky, w, kh);
			ky += kh + 1;
		}
	}

	if(m_type == Type::Widget) {

		assert(m_widget);

		m_background_alpha = std::clamp(m_background_alpha, 0.0f, 0.2f);

		// setup window
		ImGuiWindowFlags flags = 0;
		flags |= ImGuiWindowFlags_NoCollapse;
		flags |= ImGuiWindowFlags_NoMove;
		flags |= ImGuiWindowFlags_NoTitleBar;
		flags |= ImGuiWindowFlags_NoSavedSettings;
		flags |= ImGuiWindowFlags_NoNavInputs;
		flags |= ImGuiWindowFlags_NoScrollbar;

		ImGui::SetNextWindowPos(ImVec2((float)x, (float)y));
		ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
		ImGui::SetNextWindowBgAlpha(m_background_alpha);

		if(m_has_focus) {
			ImGui::PushStyleColor(ImGuiCol_Border, (ImU32)Style::color(Style::ColorId::PanelBorder));
			ImGui::Begin(m_title, nullptr, flags);
			ImGui::PopStyleColor();
			m_background_alpha -= 0.03;
		} else {
			m_background_alpha += 0.03;
			ImGui::Begin(m_title, nullptr, flags);
		}

		m_has_focus = ImGui::IsWindowFocused();

		if(ImGui::IsWindowHovered() &&
		   !ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
		   !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
			ImGui::SetWindowFocus();
		}

		Widget *widget_new = Widgets::draw(m_widget->name());
		if(widget_new) {
			m_widget->copy_to(widget_new);
			delete m_widget;
			m_widget = widget_new;
		}


		// Handle keyboard shortcuts
		if(ImGui::IsWindowFocused()) {
			if(ImGui::IsKeyPressed(ImGuiKey_V)) {
				if(m_parent->type() == Type::SplitV) {
					Panel *pn = new Panel(m_widget->copy());
					m_parent->add(pn, this);
				} else {
					Panel *pnew = new Panel(Type::SplitV);
					m_parent->replace(this, pnew);
					pnew->add(this);
					pnew->add(new Panel(m_widget->copy()), this);
				}
			}
			if(ImGui::IsKeyPressed(ImGuiKey_H)) {
				if(m_parent->type() == Type::SplitH) {
					Panel *pn = new Panel(m_widget->copy());
					m_parent->add(pn, this);
				} else {
					Panel *pnew = new Panel(Type::SplitH);
					m_parent->replace(this, pnew);
					pnew->add(this);
					pnew->add(new Panel(m_widget->copy()), this);
				}
			}
			if(ImGui::IsKeyPressed(ImGuiKey_X)) {
				if(m_parent && m_parent->type() != Type::Root) {
					m_parent->remove(this);
				}
			}
		}

		// Handle window move/resize
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		if(pos.x != x || pos.y != y || size.x != w || size.y != h) {
			int dx1 = pos.x - x;
			int dy1 = pos.y - y;
			int dx2 = (pos.x + size.x) - (x + w);
			int dy2 = (pos.y + size.y) - (y + h);
			m_parent->update_kid(this, dx1, dy1, dx2, dy2);
		}
	
		// draw widget
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		SDL_Rect r = { (int)cursor.x, (int)cursor.y, (int)avail.x, (int)avail.y };
		SDL_SetRenderClipRect(rend, &r);
		m_widget->draw(view, exp, rend, r);
		SDL_SetRenderClipRect(rend, nullptr);

		ImGui::End();

	} 

	// remove kids marked for deletion
	for(auto &pk : m_kids_remove) {
		for(size_t i=0; i<m_kids.size(); i++) {
			if(m_kids[i] == pk) {
				m_kids.erase(m_kids.begin() + i);
				pk->m_parent = nullptr;
				delete pk;
			}
		}
	}
	m_kids_remove.clear();

	// add kids marked for addition
	for(auto &req : m_add_requests) {
		if(req.p_after) {
			for(size_t i=0; i<m_kids.size(); i++) {
				if(m_kids[i] == req.p_after) {
					m_kids.insert(m_kids.begin() + i + 1, req.p_new);
					req.p_new->m_parent = this;
					// take half the weight of the panel after which we are inserting
					req.p_after->m_weight *= 0.5f;
					req.p_new->m_weight = req.p_after->m_weight;
				}
			}
		} else {
			m_kids.push_back(req.p_new);
		}
		req.p_new->m_parent = this;
	}
	m_add_requests.clear();

	// normalize weights
	float total_weight = 0.0f;
	for(auto &k : m_kids) total_weight += k->m_weight;
	for(auto &k : m_kids) k->m_weight /= total_weight;

	// if only one kid left, collapse
	if(m_kids.size() == 1 && m_parent) {
		Panel *kid = m_kids[0];
		m_parent->replace(this, kid);
		m_kids.clear();
		delete this;
	}
}

void Panel::dump(int depth)
{
	printf("%p ", this);
	for(int i=0; i<depth; i++) {
		printf("  ");
	}
	if(m_type == Type::Root) printf("root");
	if(m_type == Type::SplitH) printf("split_h");
	if(m_type == Type::SplitV) printf("split_v");
	if(m_type == Type::Widget) printf("widget");
	printf(" %p\n", m_parent);
	for(auto &k : m_kids) {
		k->dump(depth + 1);
	}
}
