#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"
#include "simcontext.hpp"

class WidgetDummy : public Widget {
public:
    WidgetDummy(Widget::Info &info) : Widget(info) {}

    void do_draw(SimContext &ctx, SDL_Renderer *rend, SDL_Rect &r) override {
        SDL_SetRenderDrawColor(rend, 40, 60, 80, 255);
        SDL_RenderFillRect(rend, nullptr);

        ImGui::Text("Quantum Simulator");
        ImGui::Text("Panel: %dx%d", r.w, r.h);
    }
};

REGISTER_WIDGET(WidgetDummy,
    .name = "dummy",
    .description = "Empty placeholder widget",
)
