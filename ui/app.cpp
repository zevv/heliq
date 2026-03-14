#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "app.hpp"
#include "style.hpp"


App::App()
{
    resize_window(800, 600);
}


void App::config_fname(char *buf, size_t buflen)
{
	char dir[PATH_MAX];
	const char *path = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	if(path) {
		snprintf(dir, sizeof(dir), "%s/quantum", path);
	} else if(home) {
		snprintf(dir, sizeof(dir), "%s/.config/quantum", home);
	} else {
		snprintf(dir, sizeof(dir), "./.quantum");
	}
	mkdir(dir, 0755);
	snprintf(buf, buflen, "%s/%s", dir, m_session_name);
}


void App::load()
{
	char fname[PATH_MAX];
	config_fname(fname, sizeof(fname));

	ConfigReader cr;
	cr.open(fname);
	if(auto n = cr.find("panel")) m_root_panel->load(n);
	if(auto n = cr.find("style")) Style::load(n);
}


void App::save()
{
	char fname[PATH_MAX];
	config_fname(fname, sizeof(fname));

	ConfigWriter cw;
	cw.open(fname);
	cw.push("panel");
	m_root_panel->save(cw);
	cw.pop();
	cw.push("style");
	Style::save(cw);
	cw.pop();
	cw.close();
}


void App::init_video()
{
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_win = SDL_CreateWindow("Quantum Simulator", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if(m_win == nullptr) {
        fprintf(stderr, "Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        ::exit(1);
    }
    m_rend = SDL_CreateRenderer(m_win, nullptr);
    SDL_SetRenderVSync(m_rend, 1);
    if(m_rend == nullptr) {
        fprintf(stderr, "Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        ::exit(1);
    }
    SDL_SetWindowPosition(m_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_win);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = NULL;
    io.LogFilename = NULL;

	struct stat st;
	if(stat("Lato-Regular.ttf", &st) == 0) {
		m_font = io.Fonts->AddFontFromFileTTF("Lato-Regular.ttf", 14.0);
	}

    ImGui_ImplSDL3_InitForSDLRenderer(m_win, m_rend);
    ImGui_ImplSDLRenderer3_Init(m_rend);
}


void App::resize_window(int w, int h)
{
	m_w = w;
	m_h = h;
	m_resize = true;
}


void App::req_redraw()
{
	m_redraw = 40;
}


int App::draw_topbar()
{
	ImGuiWindowFlags flags = 0;
	flags |= ImGuiWindowFlags_NoCollapse;
	flags |= ImGuiWindowFlags_NoMove;
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoSavedSettings;
	flags |= ImGuiWindowFlags_NoScrollbar;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(m_w, 20));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::Begin("topbar", nullptr, flags);

	ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);

	ImGui::End();
	return 20;
}


void App::draw_bottombar()
{
	ImGuiWindowFlags flags = 0;
	flags |= ImGuiWindowFlags_NoCollapse;
	flags |= ImGuiWindowFlags_NoMove;
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoSavedSettings;
	flags |= ImGuiWindowFlags_NoScrollbar;

	ImGui::SetNextWindowPos(ImVec2(0, m_h - 20));
	ImGui::SetNextWindowSize(ImVec2(m_w, 20));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::Begin("bottombar", nullptr, flags);

	ImGui::Text("Quantum Simulator - Press Q to quit");

	ImGui::End();
}


void App::draw()
{
	SDL_SetRenderDrawColor(m_rend, 0, 0, 0, 255);
	SDL_RenderClear(m_rend);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if(m_font) ImGui::PushFont(m_font);

	int bar_h = draw_topbar();
	draw_bottombar();
	m_root_panel->draw(m_view, m_experiment, m_rend, 0, bar_h + 1, m_w, m_h - bar_h - bar_h + 1);

	if(m_font) ImGui::PopFont();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_rend);
	SDL_RenderPresent(m_rend);
}


void App::init(int argc, char **argv)
{
	init_video();
	
	m_root_panel = new Panel(Panel::Type::Root);
	load();

	// Create default panel layout if empty
	if(m_root_panel->nkids() == 0) {
		Panel *p1 = new Panel(Panel::Type::SplitV);
		p1->add(Widgets::create_widget("dummy"));
		m_root_panel->add(p1);
	}
}


void App::run()
{
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		SDL_WaitEvent(&event);
		do {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				done = true;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && 
				event.window.windowID == SDL_GetWindowID(m_win))
				done = true;
			if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_Q)
				done = true;
			if(event.type == SDL_EVENT_WINDOW_RESIZED && 
			   event.window.windowID == SDL_GetWindowID(m_win))
				resize_window(event.window.data1, event.window.data2);

			req_redraw();
		}
		while (SDL_PollEvent(&event));

		if(m_redraw > 0) {
			draw();
			m_redraw --;
		} else {
			SDL_Delay(10);
		}
	}
}


void App::exit()
{
	save();
	delete m_root_panel;
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer(m_rend);
	SDL_DestroyWindow(m_win);
}
