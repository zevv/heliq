#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include "config.hpp"
#include "grid.hpp"

class View {
public:
    enum class Axis { None };

    struct Config {
        Axis x{Axis::None};
        Axis y{Axis::None};
    };

    void load(ConfigReader::Node *n) {}
    void save(ConfigWriter &cfg) {}

    bool lock{true};

    // shared slice state — helix writes, grid reads
    int slice_axis{0};
    int slice_pos[MAX_RANK]{};
    bool slice_valid{false};
};
