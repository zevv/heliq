#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include "config.hpp"
#include "grid.hpp"
#include "camera3d.hpp"

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

    // shared camera state for locked helix widgets
    Camera3D camera;
    float amplitude{0.1f};

    // shared slice state for locked helix widgets (mode and axis stay per-widget)
    bool normalize{false};
    bool auto_track{false};

    // persistent grid cursor — set by grid LMB click, read by helix widgets
    int cursor[MAX_RANK]{};

    // shared spatial zoom/pan — screen-fraction coordinates
    // sl = left edge of spatial axis as fraction of panel width [0..1]
    // sr = right edge of spatial axis as fraction of panel width [0..1]
    // both helix and trace read/write these when locked
    float spatial_sl{0}, spatial_sr{1};

    // per-frame slice list — rebuilt each frame by helix widgets, read by grid for crosshairs
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
