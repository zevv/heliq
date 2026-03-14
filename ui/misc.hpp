
#pragma once

#include <SDL3/SDL.h>

#define CONCAT(lhs, rhs) lhs # rhs
#define CONCAT_WRAPPER(lhs, rhs) CONCAT(lhs, rhs)
#define UNIQUE_ID CONCAT_WRAPPER(__FILE__, __LINE__)

double hirestime();

void humanize(double val, char *buf, size_t buf_len);

namespace ImGui {
	bool ToggleButton(const char* str_id, bool* v);
	bool IsMouseInRect(const SDL_Rect& r);
    void TextShadow(const char* fmt, ...);
    bool SliderDouble(const char* label, double* v, double v_min, double v_max, const char* format = "%.3f", int flags = 0);
    bool DragDouble(const char* label, double* v, double v_speed = 1.0f, double v_min = 0.0f, double v_max = 0.0f, const char* format = "%.3f", int flags = 0);
}
