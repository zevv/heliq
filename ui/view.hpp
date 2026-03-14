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

    // shared slice state — helix widgets write, grid reads
    static constexpr int MAX_SLICES = 8;
    struct Slice {
        int axis{};
        int pos[MAX_RANK]{};
        bool valid{false};
    };
    int n_slices{};
    Slice slices[MAX_SLICES]{};

    void clear_slices() { n_slices = 0; }
    void add_slice(int axis, const int *pos) {
        if(n_slices >= MAX_SLICES) return;
        auto &s = slices[n_slices++];
        s.axis = axis;
        for(int d = 0; d < MAX_RANK; d++) s.pos[d] = pos[d];
        s.valid = true;
    }
};
