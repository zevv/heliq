#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>
#include <math.h>
#include "math3d.hpp"
#include "config.hpp"

static constexpr double CAM_PITCH_LIMIT = M_PI * 0.49;

struct Camera3D {
	double yaw{0}, pitch{0}, dist{2.5};
	double pan_x{0}, pan_y{0};
	bool ortho{true};

	bool orbiting{false}, panning{false};
	float drag_x{}, drag_y{};

	mat4 build(int w, int h) const {
		vec3 center = {pan_x, pan_y, 0};
		vec3 eye = {
			center.x + dist * sin(yaw) * cos(pitch),
			center.y + dist * sin(pitch),
			center.z + dist * cos(yaw) * cos(pitch),
		};
		mat4 view = mat4::look_at(eye, center, {0, 1, 0});
		double aspect = (double)w / h;
		mat4 proj = ortho
			? mat4::ortho(dist * 0.5, aspect, -100.0, 100.0)
			: mat4::perspective(0.8, aspect, 0.001, 1000.0);
		return proj * view;
	}

	void handle_mouse(SDL_Rect &r) {
		ImVec2 mp = ImGui::GetMousePos();
		bool in_rect = mp.x >= r.x && mp.x < r.x + r.w &&
		               mp.y >= r.y && mp.y < r.y + r.h;
		bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

		// Shift+RMB = orbit, RMB = pan
		if(in_rect && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			if(shift) orbiting = true;
			else      panning = true;
			drag_x = mp.x; drag_y = mp.y;
		}
		if(orbiting && ImGui::IsMouseDown(ImGuiMouseButton_Right) && shift) {
			yaw   -= (mp.x - drag_x) * 0.005;
			pitch += (mp.y - drag_y) * 0.005;
			if(pitch >  CAM_PITCH_LIMIT) pitch =  CAM_PITCH_LIMIT;
			if(pitch < -CAM_PITCH_LIMIT) pitch = -CAM_PITCH_LIMIT;
			drag_x = mp.x; drag_y = mp.y;
		}
		if(panning && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			double hfov_half = 0.4;  // half of 0.8 rad horizontal FOV
			double visible_w = ortho ? dist * 0.5 * 2.0 : 2.0 * dist * tan(hfov_half);
			double scale = visible_w / r.w;
			pan_x -= (mp.x - drag_x) * scale * cos(yaw);
			pan_y += (mp.y - drag_y) * scale;
			drag_x = mp.x; drag_y = mp.y;
		}
		if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			orbiting = false; panning = false;
		}

		if(in_rect) {
			float wheel = ImGui::GetIO().MouseWheel;
			if(wheel != 0) {
				dist *= (1.0 - wheel * 0.1);
				if(dist < 0.1) dist = 0.1;
				if(dist > 50.0) dist = 50.0;
			}
		}
	}

	void save(ConfigWriter &cfg, const char *prefix = "") const {
		char k[32];
		auto key = [&](const char *name) -> const char * {
			snprintf(k, sizeof(k), "%s%s", prefix, name); return k;
		};
		cfg.write(key("yaw"), yaw);
		cfg.write(key("pitch"), pitch);
		cfg.write(key("dist"), dist);
		cfg.write(key("pan_x"), pan_x);
		cfg.write(key("pan_y"), pan_y);
		cfg.write(key("ortho"), ortho ? 1 : 0);
	}

	void load(ConfigReader::Node *node, const char *prefix = "") {
		if(!node) return;
		char k[32];
		auto key = [&](const char *name) -> const char * {
			snprintf(k, sizeof(k), "%s%s", prefix, name); return k;
		};
		node->read(key("yaw"), yaw);
		node->read(key("pitch"), pitch);
		node->read(key("dist"), dist);
		node->read(key("pan_x"), pan_x);
		node->read(key("pan_y"), pan_y);
		int o = ortho; node->read(key("ortho"), o); ortho = o;
	}

	void handle_keys() {
		auto key = [](ImGuiKey numpad, ImGuiKey regular) {
			return ImGui::IsKeyPressed(numpad) || ImGui::IsKeyPressed(regular);
		};
		bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

		if(key(ImGuiKey_Keypad1, ImGuiKey_1)) {
			if(ctrl) { yaw = M_PI; pitch = 0; }
			else     { yaw = 0;    pitch = 0; }
		}
		if(key(ImGuiKey_Keypad3, ImGuiKey_3)) {
			if(ctrl) { yaw = -M_PI/2; pitch = 0; }
			else     { yaw =  M_PI/2; pitch = 0; }
		}
		if(key(ImGuiKey_Keypad7, ImGuiKey_7)) {
			if(ctrl) { yaw = 0; pitch = -CAM_PITCH_LIMIT; }
			else     { yaw = 0; pitch =  CAM_PITCH_LIMIT; }
		}
		if(key(ImGuiKey_Keypad5, ImGuiKey_5)) { ortho = !ortho; }

		if(key(ImGuiKey_Keypad4, ImGuiKey_4)) { yaw -= M_PI/12; }
		if(key(ImGuiKey_Keypad6, ImGuiKey_6)) { yaw += M_PI/12; }
		if(key(ImGuiKey_Keypad8, ImGuiKey_8)) {
			pitch += M_PI/12;
			if(pitch > CAM_PITCH_LIMIT) pitch = CAM_PITCH_LIMIT;
		}
		if(key(ImGuiKey_Keypad2, ImGuiKey_2)) {
			pitch -= M_PI/12;
			if(pitch < -CAM_PITCH_LIMIT) pitch = -CAM_PITCH_LIMIT;
		}
	}
};
