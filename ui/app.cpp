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
	if(auto n = cr.find("experiment")) {
		double ts = m_experiment.timescale;
		n->read("timescale", ts);
		m_experiment.timescale = ts;
		if(!m_experiment.simulations.empty()) {
			double dt = m_experiment.simulations[0]->dt;
			n->read("dt", dt);
			for(auto &sim : m_experiment.simulations)
				sim->set_dt(dt);
			int ab = 0;
			n->read("absorb", ab);
			if(ab) {
				for(auto &sim : m_experiment.simulations)
					sim->set_absorbing_boundary(true);
			}
		}
		for(int d = 0; d < MAX_RANK; d++) {
			char key[16]; snprintf(key, sizeof(key), "cursor_%d", d);
			n->read(key, m_view.cursor[d]);
		}
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
	cw.write("timescale", m_experiment.timescale);
	if(!m_experiment.simulations.empty()) {
		cw.write("dt", m_experiment.simulations[0]->dt);
		cw.write("absorb", m_experiment.simulations[0]->absorbing_boundary ? 1 : 0);
	}
	for(int d = 0; d < MAX_RANK; d++) {
		char key[16]; snprintf(key, sizeof(key), "cursor_%d", d);
		cw.write(key, m_view.cursor[d]);
	}
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

	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("FPS: %.0f", fps);

	ImGui::SameLine();
	ImGui::Text("t=%.3e s", m_experiment.sim_time);

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
	m_view.clear_slices();
	m_root_panel->draw(m_view, m_experiment, m_rend, 0, bar_h + 1, m_w, m_h - bar_h - bar_h + 1);

	if(m_font) ImGui::PopFont();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_rend);
	SDL_RenderPresent(m_rend);
}


void App::init(int argc, char **argv)
{
	init_video();

	// load experiment from script
	const char *script = (argc > 1) ? argv[1] : "experiments/barrier-1d.lua";
	if(load_setup(script, m_experiment.setup, true)) {
		auto &s = m_experiment.setup;
		fprintf(stderr, "loaded: %dD, %zu particles, %zu potentials, %zu sims\n",
			s.spatial_dims, s.particles.size(), s.potentials.size(), s.simulations.size());
		m_experiment.timescale = s.timescale;
		// create simulation instances
		for(auto &sc : s.simulations) {
			m_experiment.simulations.push_back(
				std::make_unique<Simulation>(sc, s));
		}
	} else {
		fprintf(stderr, "failed to load experiment\n");
	}

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

		// advance simulation
		double wall_dt = 1.0 / ImGui::GetIO().Framerate;
		if(wall_dt > 0.1) wall_dt = 1.0 / 60.0;  // clamp on first frame
		m_experiment.advance(wall_dt);

		// spacebar to toggle play/pause
		if(ImGui::IsKeyPressed(ImGuiKey_Space))
			m_experiment.running = !m_experiment.running;

		// / to reverse time direction
		if(ImGui::IsKeyPressed(ImGuiKey_Slash))
			m_experiment.timescale = -m_experiment.timescale;

		// B to toggle absorbing boundary
		if(ImGui::IsKeyPressed(ImGuiKey_B)) {
			for(auto &sim : m_experiment.simulations)
				sim->set_absorbing_boundary(!sim->absorbing_boundary);
		}

		// [ and ] adjust timescale by 10^(1/3) ~= 2.154x
		// shift+[ and shift+] adjust dt
		{
			double factor = pow(10.0, 1.0/3.0);
			bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
			if(ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
				if(shift) {
					for(auto &sim : m_experiment.simulations)
						sim->set_dt(sim->dt * factor);
				} else {
					m_experiment.timescale *= factor;
				}
			}
			if(ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
				if(shift) {
					for(auto &sim : m_experiment.simulations)
						sim->set_dt(sim->dt / factor);
				} else {
					m_experiment.timescale /= factor;
				}
			}
		}

		// dump state to file
		if(ImGui::IsKeyPressed(ImGuiKey_D)) {
			if(!m_experiment.simulations.empty()) {
				auto &sim = *m_experiment.simulations[0];
				auto *psi = sim.psi_front();
				auto *pot = sim.potential;
				int nx = sim.grid.axes[0].points;
				int ny = (sim.grid.rank >= 2) ? sim.grid.axes[1].points : 1;

				// downsample: 2x horizontal resolution for aspect ratio
				int sx = (nx + 255) / 256;
				int sy = (ny + 127) / 128;
				int ox = nx / sx;
				int oy = ny / sy;

				// find max for normalization
				double max_val = 1e-30;
				for(int i = 0; i < nx * ny; i++) {
					double v = std::norm(psi[i]);
					if(v > max_val) max_val = v;
				}

				FILE *f = fopen("dump.txt", "w");
				fprintf(f, "# t=%.4e  nx=%d ny=%d  downsampled %dx%d  max=%.4e\n",
					sim.time(), nx, ny, ox, oy, max_val);

				// |psi|^2 with potential overlay
				fprintf(f, "# |psi|^2 (potential shown as ||):\n");
				const char *shades = "_.:-=o+*#%@";
				int nshades = 11;
				for(int iy = oy - 1; iy >= 0; iy--) {
					for(int ix = 0; ix < ox; ix++) {
						int gx = ix * sx + sx/2;
						int gy = iy * sy + sy/2;
						size_t idx = (size_t)gx * ny + gy;
						if(pot[idx].real() > 0) {
							fputc('|', f);
						} else {
							double v = pow(std::norm(psi[idx]) / max_val, 0.15);
							int si = (int)(v * (nshades - 1));
							if(si >= nshades) si = nshades - 1;
							if(si < 0) si = 0;
							fputc(shades[si], f);
						}
					}
					fputc('\n', f);
				}

				fclose(f);
				fprintf(stderr, "dumped state to dump.txt (%dx%d)\n", ox, oy);
			}
		}

		// single step with right arrow
		if(ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !m_experiment.running) {
			for(auto &sim : m_experiment.simulations)
				sim->step();
			m_experiment.sim_time = m_experiment.simulations.empty() ? 0 :
				m_experiment.simulations[0]->time();
		}

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
