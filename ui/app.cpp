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
#include "loader.hpp"
#include "misc.hpp"


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
		snprintf(dir, sizeof(dir), "%s/heliq", path);
	} else if(home) {
		snprintf(dir, sizeof(dir), "%s/.config/heliq", home);
	} else {
		snprintf(dir, sizeof(dir), "./.heliq");
	}
	mkdir(dir, 0755);

	// derive session name from script basename (without extension)
	const char *base = m_script.c_str();
	const char *slash = strrchr(base, '/');
	if(slash) base = slash + 1;
	char name[64];
	snprintf(name, sizeof(name), "%s", base);
	char *dot = strrchr(name, '.');
	if(dot) *dot = '\0';

	snprintf(buf, buflen, "%s/%s", dir, name);
}


void App::load()
{
	char fname[PATH_MAX];
	config_fname(fname, sizeof(fname));

	ConfigReader cr;
	cr.open(fname);
	if(auto n = cr.find("panel")) m_root_panel->load(n);
	if(auto n = cr.find("style")) Style::load(n);
	if(auto n = cr.find("experiment")) {
		double ts = m_ctx.state().timescale;
		n->read("timescale", ts);
		m_ctx.push(CmdSetTimescale{fabs(ts)});
		double dt = m_ctx.state().dt;
		n->read("dt", dt);
		if(dt > 0) m_ctx.push(CmdSetDt{dt});
		for(int d = 0; d < MAX_RANK; d++) {
			char key[16]; snprintf(key, sizeof(key), "cursor_%d", d);
			n->read(key, m_view.cursor[d]);
		}
		m_view.camera.load(n, "cam_");
		n->read("amplitude", m_view.amplitude);
		double scale = m_ui_scale;
		n->read("ui_scale", scale);
		m_ui_scale = (float)scale;
		int norm = 0, autotr = 0;
		n->read("normalize", norm); m_view.normalize = norm;
		n->read("auto_track", autotr); m_view.auto_track = autotr;
	}
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
	cw.push("experiment");
	cw.write("timescale", m_ctx.state().timescale);
	if(m_ctx.state().dt > 0)
		cw.write("dt", m_ctx.state().dt);
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "cursor_%d", d);
		cw.write(key, m_view.cursor[d]);
	}
	m_view.camera.save(cw, "cam_");
	cw.write("amplitude", m_view.amplitude);
	cw.write("normalize", m_view.normalize ? 1 : 0);
	cw.write("auto_track", m_view.auto_track ? 1 : 0);
	cw.write("ui_scale", (double)m_ui_scale);
	cw.pop();
	cw.close();
}


void App::init_cursor()
{
	auto &st = m_ctx.state();
	auto &gm = st.grid;
	if(gm.rank == 0) return;

	// set cursor to initial particle positions
	for(int p = 0; p < gm.cs.n_particles; p++) {
		for(int d = 0; d < gm.cs.spatial_dims; d++) {
			int ax = gm.cs.axis(p, d);
			if(ax >= gm.rank) continue;
			auto &axis = gm.axes[ax];
			double pos = (p < (int)st.setup.particles.size()) ? st.setup.particles[p].position[d] : 0;
			int idx = (int)((pos - axis.min) / (axis.max - axis.min) * axis.points);
			if(idx < 0) idx = 0;
			if(idx >= axis.points) idx = axis.points - 1;
			m_view.cursor[ax] = idx;
		}
	}
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
    fprintf(stderr, "SDL renderer: %s\n", SDL_GetRendererName(m_rend));
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
	flags |= ImGuiWindowFlags_NoDecoration;

	float pad = 2.0f * m_ui_scale;
	float bar_h = ImGui::GetFontSize() + pad * 2;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, pad));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(m_w, bar_h));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::Begin("topbar", nullptr, flags);

	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("FPS: %.0f", fps);

	auto &st = m_ctx.state();
	ImGui::SameLine();
	char time_str[64];
	humanize_unit(st.sim_time, "s", time_str, sizeof(time_str));
	ImGui::Text("t=%s", time_str);

	// right-aligned sanity checks
	if(st.grid.rank > 0) {
		ImVec4 col_ok   = ImVec4(0.4, 0.8, 0.4, 1);
		ImVec4 col_warn = ImVec4(0.9, 0.8, 0.2, 1);
		ImVec4 col_bad  = ImVec4(1.0, 0.3, 0.3, 1);

		float rx = ImGui::GetWindowWidth() - 8;

		for(int d = st.grid.rank - 1; d >= 0; d--) {
			double kr = st.k_nyquist_ratio[d];
			ImVec4 col = (kr < 0.3) ? col_ok : (kr < 0.5) ? col_warn : col_bad;
			char buf[32];
			snprintf(buf, sizeof(buf), "%d:%.0f%%", d, kr * 100);
			rx -= ImGui::CalcTextSize(buf).x + 4;
			ImGui::SameLine(rx);
			ImGui::TextColored(col, "%s", buf);
		}

		rx -= ImGui::CalcTextSize("Grid").x + 8;
		ImGui::SameLine(rx);
		ImGui::Text("Grid");

		double pp = st.phase_v;
		double kp = st.phase_k;
		ImVec4 col_p = (pp < 0.3) ? col_ok : (pp < 1.0) ? col_warn : col_bad;
		ImVec4 col_k = (kp < 0.3) ? col_ok : (kp < 1.0) ? col_warn : col_bad;

		char kbuf[32], pbuf[32];
		snprintf(kbuf, sizeof(kbuf), "K %.2f", kp);
		snprintf(pbuf, sizeof(pbuf), "V %.2f", pp);

		rx -= ImGui::CalcTextSize(kbuf).x + 4;
		ImGui::SameLine(rx);
		ImGui::TextColored(col_k, "%s", kbuf);

		rx -= ImGui::CalcTextSize(pbuf).x + 4;
		ImGui::SameLine(rx);
		ImGui::TextColored(col_p, "%s", pbuf);

		rx -= ImGui::CalcTextSize("Phase").x + 8;
		ImGui::SameLine(rx);
		ImGui::Text("Phase");

		double prob = st.total_probability;
		ImVec4 col_prob = (fabs(prob - 1.0) < 0.01) ? col_ok :
		                  (fabs(prob - 1.0) < 0.05) ? col_warn : col_bad;
		char prob_buf[32];
		snprintf(prob_buf, sizeof(prob_buf), "P=%.4f", prob);
		rx -= ImGui::CalcTextSize(prob_buf).x + 8;
		ImGui::SameLine(rx);
		ImGui::TextColored(col_prob, "%s", prob_buf);
	}

	ImGui::End();
	ImGui::PopStyleVar(3);
	return (int)bar_h;
}


int App::draw_bottombar()
{
	ImGuiWindowFlags flags = 0;
	flags |= ImGuiWindowFlags_NoCollapse;
	flags |= ImGuiWindowFlags_NoMove;
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoSavedSettings;
	flags |= ImGuiWindowFlags_NoScrollbar;

	float pad = 2.0f * m_ui_scale;
	float bar_h = ImGui::GetFontSize() + pad * 2;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, pad));
	ImGui::SetNextWindowPos(ImVec2(0, m_h - bar_h));
	ImGui::SetNextWindowSize(ImVec2(m_w, bar_h));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::Begin("bottombar", nullptr, flags);

	ImGui::Text("Quantum Simulator - Press Q to quit");

	ImGui::End();
	ImGui::PopStyleVar();
	return (int)bar_h;
}


void App::draw()
{
	SDL_SetRenderDrawColor(m_rend, 0, 0, 0, 255);
	SDL_RenderClear(m_rend);

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if(m_font) ImGui::PushFont(m_font);

	int top_h = draw_topbar();
	int bot_h = draw_bottombar();
	m_view.clear_slices();
	m_root_panel->draw(m_view, m_ctx, m_rend, 0, top_h + 1, m_w, m_h - top_h - bot_h - 2);

	if(m_font) ImGui::PopFont();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_rend);
	SDL_RenderPresent(m_rend);
}


void App::init(int argc, char **argv)
{
	init_video();

	// load experiment from script
	m_script = (argc > 1) ? argv[1] : "experiments/010-1D-1P-momentum.lua";
	{
		Setup s{};
		load_setup(m_script.c_str(), s, true);
		m_ctx.push(CmdLoad{std::move(s)});
	}
	m_ctx.poll(0);
	init_cursor();

	m_root_panel = new Panel(Panel::Type::Root);
	load();

	// Create default panel layout if empty
	if(m_root_panel->nkids() == 0) {
		Panel *split = new Panel(Panel::Type::SplitH);
		Panel *left = new Panel(Panel::Type::SplitV);
		Panel *right = new Panel(Panel::Type::SplitV);
		left->add(Widgets::create_widget("helix"));
		right->add(Widgets::create_widget("info"));
		split->add(left);
		split->add(right);
		m_root_panel->add(split);
	}
}


void App::run()
{
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
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
		}

		// pump model: drain commands, advance, extract, publish
		double wall_dt = 1.0 / ImGui::GetIO().Framerate;
		if(wall_dt > 0.1) wall_dt = 1.0 / 60.0;  // clamp on first frame
		m_ctx.poll(wall_dt);

		auto &st = m_ctx.state();

		// spacebar to toggle play/pause
		if(ImGui::IsKeyPressed(ImGuiKey_Space))
			m_ctx.push(CmdSetRunning{!st.running});

		// , for reverse, . for forward
		if(ImGui::IsKeyPressed(ImGuiKey_Comma))
			m_ctx.push(CmdSetTimescale{-fabs(st.timescale)});
		if(ImGui::IsKeyPressed(ImGuiKey_Period))
			m_ctx.push(CmdSetTimescale{fabs(st.timescale)});

		// R to reload experiment
		if(ImGui::IsKeyPressed(ImGuiKey_R)) {
			Setup s{};
			load_setup(m_script.c_str(), s, true);
			m_ctx.push(CmdLoad{std::move(s)});
		}

		// B to toggle absorbing boundary
		if(ImGui::IsKeyPressed(ImGuiKey_B))
			m_ctx.push(CmdSetAbsorb{!st.absorbing_boundary, (float)st.absorb_width, (float)st.absorb_strength});

		// [ and ] adjust timescale by 10^(1/3) ~= 2.154x
		// shift+[ and shift+] adjust dt
		{
			double factor = pow(10.0, 1.0/3.0);
			bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
			if(ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
				if(shift)
					m_ctx.push(CmdSetDt{st.dt * factor});
				else
					m_ctx.push(CmdSetTimescale{st.timescale * factor});
			}
			if(ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
				if(shift)
					m_ctx.push(CmdSetDt{st.dt / factor});
				else
					m_ctx.push(CmdSetTimescale{st.timescale / factor});
			}
		}



		// Ctrl+0/+/- for UI scale
		bool ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
		if(ctrl && ImGui::IsKeyPressed(ImGuiKey_0))
			m_ui_scale = 1.0f;
		if(ctrl && ImGui::IsKeyPressed(ImGuiKey_Equal))
			m_ui_scale *= 1.2f;
		if(ctrl && ImGui::IsKeyPressed(ImGuiKey_Minus))
			m_ui_scale /= 1.2f;
		if(m_ui_scale < 0.4f) m_ui_scale = 0.4f;
		if(m_ui_scale > 4.0f) m_ui_scale = 4.0f;
		ImGui::GetIO().FontGlobalScale = m_ui_scale;

		// single step with right arrow
		if(ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !st.running)
			m_ctx.push(CmdSingleStep{});

		draw();
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
