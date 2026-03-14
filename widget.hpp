#pragma once

#include <SDL3/SDL.h>

#include "view.hpp"
#include "config.hpp"

class Simulation;

class Widget {
public:
    struct Info {
        const char *name{};
        const char *description{};
        enum ImGuiKey hotkey{};
        Widget *(*fn_new)();
    };

    Widget(Info &info);
    virtual ~Widget();

    void load(ConfigReader::Node *node);
    void save(ConfigWriter &cfg);
    Widget *copy();
    void copy_to(Widget *w);
    const char *name() { return m_info.name; }
    void draw(View &view, Simulation &sim, SDL_Renderer *rend, SDL_Rect &r);

protected:
    virtual void do_copy(Widget *w) {};
    virtual void do_draw(Simulation &sim, SDL_Renderer *rend, SDL_Rect &r) {};
    virtual void do_load(ConfigReader::Node *node) {};
    virtual void do_save(ConfigWriter &cfg) {};

    Info &m_info;
    View m_view{};
};
