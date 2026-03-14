#include <time.h>
#include <math.h>
#include <stdio.h>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "misc.hpp"
#include "style.hpp"


double hirestime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}


void humanize(double val, char *buf, size_t buf_len)
{
	struct scale {
		double v;
		const char *suffix;
	} scales[] = {
		{ 1e12, "T" },
		{ 1e9,  "G" },
		{ 1e6,  "M" },
		{ 1e3,  "k" },
		{ 1.0,  "" },
		{ 1e-3, "m" },
		{ 1e-6, "μ" },
		{ 1e-9, "n" },
		{ 1e-12,"p" },
		{ 0.0,  nullptr }
	};

	for(int i=0; scales[i].suffix != nullptr; i++) {
		if(fabs(val) >= scales[i].v * 0.99) {
			double v = val / scales[i].v;
			snprintf(buf, buf_len, "%.5g %s", v, scales[i].suffix);
			return;
		}
	}
}


namespace ImGui {
bool ToggleButton(const char* str_id, bool* v)
{
	ImVec4 col = Style::color(*v ? Style::ColorId::ToggleButtonOn : Style::ColorId::ToggleButtonOff);
	ImGui::PushStyleColor(ImGuiCol_Button, col);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
	bool pressed = ImGui::Button(str_id);
	if(pressed) *v = !*v;
	ImGui::PopStyleColor(3);
	return pressed;
}


bool IsMouseInRect(SDL_Rect const &rect)
{
	ImVec2 mp = ImGui::GetIO().MousePos;
	return (mp.x >= rect.x) && (mp.x < rect.x + rect.w) &&
	       (mp.y >= rect.y) && (mp.y < rect.y + rect.h);
}
    
void TextShadow(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buf[256];
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	ImVec2 text_size = ImGui::CalcTextSize(buf);
	auto draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(ImGui::GetCursorScreenPos(),
			ImVec2(ImGui::GetCursorScreenPos().x + text_size.x,
			ImGui::GetCursorScreenPos().y + text_size.y),
			IM_COL32(0, 0, 0, 128), 0.0f);
	ImGui::TextUnformatted(buf);

}


bool SliderDouble(const char* label, double* v, double v_min, double v_max, const char* format, int flags)
{
	float v_f = static_cast<float>(*v);
	bool changed = ImGui::SliderFloat(label, &v_f, static_cast<float>(v_min), static_cast<float>(v_max), format, flags);
	if(changed) *v = static_cast<double>(v_f);
	return changed;
}


bool DragDouble(const char* label, double* v, double v_speed, double v_min, double v_max, const char* format, int flags)
{
	float v_f = static_cast<float>(*v);
	bool changed = ImGui::DragFloat(label, &v_f, static_cast<float>(v_speed), static_cast<float>(v_min), static_cast<float>(v_max), format, flags);
	if(changed) *v = static_cast<double>(v_f);
	return changed;
}

}

