
#include <assert.h>

#include "widgetregistry.hpp"

static std::vector<Widget::Info> widget_reg_list;


Widgets::Widgets(Widget::Info reg)
{
	widget_reg_list.push_back(reg);
}


Widget* Widgets::draw(const char *cur_name)
{
	Widget *rv = nullptr;

	ImGui::SetNextItemWidth(100);
	if(ImGui::BeginCombo("##widget_type", cur_name)) {
		for(auto &wi : widget_reg_list) {
			bool is_selected = strcmp(wi.name, cur_name) == 0;
			if(ImGui::Selectable(wi.name, is_selected)) {
				if(strcmp(wi.name, cur_name) != 0) {
					rv = wi.fn_new();
				}
			}
		}
		ImGui::EndCombo();
	}
		
	if(ImGui::IsWindowFocused()) {
		for(auto &wi : widget_reg_list) {
			if(wi.hotkey != ImGuiKey_None && ImGui::IsKeyPressed(wi.hotkey)) {
				if(strcmp(wi.name, cur_name) != 0) {
					return wi.fn_new();
				}
			}
		}
	}

	return rv;
}


Widget* Widgets::create_widget(const char *name)
{
	for(auto &wi : widget_reg_list) {
		if(strcmp(wi.name, name) == 0) {
			return wi.fn_new();
		}
	}
	assert(false && "unknown widget type");
}
