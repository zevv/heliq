
#pragma once

#include <vector>
#include <generator>

#include <imgui.h>

#include "widget.hpp"

class Widgets;
class Widget;


class Widgets {
public:
	Widgets(Widget::Info reg);
	static Widget *draw(const char *name);
	static Widget *create_widget(const char *name);
};


#define REGISTER_WIDGET(class, ...) \
	static Widget::Info reg = { \
		__VA_ARGS__ \
		.fn_new = []() -> Widget* { return new class(reg); }, \
	}; \
	static Widgets info(reg);


