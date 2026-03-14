#include <imgui.h>
#include <SDL3/SDL.h>
#include "widget.hpp"
#include "widgetregistry.hpp"

class WidgetDummy : public Widget {
public:
    WidgetDummy(Widget::Info &info) : Widget(info) {}

    void do_draw(Experiment &exp, SDL_Renderer *rend, SDL_Rect &r) override {
        // Draw a simple colored rectangle
        SDL_SetRenderDrawColor(rend, 40, 60, 80, 255);
        SDL_RenderFillRect(rend, nullptr);

        ImGui::Text("Quantum Simulator");
        ImGui::Text("Panel: %dx%d", r.w, r.h);
    }
};

REGISTER_WIDGET(WidgetDummy,
    .name = "dummy",
    .description = "Empty placeholder widget",
    .hotkey = ImGuiKey_F1,
)
