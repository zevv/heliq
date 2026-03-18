#pragma once

#include <string>

#include "panel.hpp"
#include "misc.hpp"
#include "view.hpp"
#include "config.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"

class App {
public:
    App();

    void init(int argc, char **argv);
    void run();
    void exit();

private:
    void init_video();
    void draw();
    int draw_topbar();
    int draw_bottombar();
    void resize_window(int w, int h);
    void req_redraw();

    void config_fname(char *buf, size_t buflen);
    void load();
    void save();
    void init_cursor();

    Panel *m_root_panel{};
    SDL_Window *m_win{};
    SDL_Renderer *m_rend{};
    View m_view{};
    SimContext m_ctx{};
    bool m_resize{true};
    int m_w{800};
    int m_h{600};
    int m_redraw{1};

    ImFont *m_font{nullptr};
    std::string m_script;
    float m_ui_scale{1.0f};
};

