#pragma once

// Central color definitions for the UI.
// All colors in one place — no magic numbers in widget code.

struct Col4f { float r, g, b, a; };
struct Col4u { unsigned char r, g, b, a; };

namespace colors {

// backgrounds
constexpr Col4f bg_gl       = { 0.04f, 0.04f, 0.06f, 1.0f };
constexpr Col4u bg_grid     = { 10, 10, 15, 255 };
constexpr Col4u bg_panel    = { 20, 20, 25, 255 };

// cursor
constexpr Col4f cursor_fill = { 0.9f, 0.2f, 0.2f, 0.15f };
constexpr Col4f cursor_edge = { 0.9f, 0.2f, 0.2f, 0.6f };
constexpr Col4u cursor_cross = { 230, 50, 50, 50 };

// grid
constexpr Col4u grid_border = { 80, 80, 80, 255 };

// absorbing boundary
constexpr Col4u absorb      = { 25, 25, 200, 255 };

// GL axes
constexpr Col4f axis_x      = { 0.5f, 0.15f, 0.15f, 0.5f };
constexpr Col4f axis_y      = { 0.15f, 0.5f, 0.15f, 0.5f };
constexpr Col4f axis_z      = { 0.15f, 0.15f, 0.5f, 0.5f };

// GL grid lines
constexpr Col4f gridline_0  = { 0.18f, 0.18f, 0.18f, 1.0f };
constexpr Col4f gridline_1  = { 0.24f, 0.24f, 0.24f, 1.0f };
constexpr Col4f gridline_2  = { 0.31f, 0.31f, 0.31f, 1.0f };

// helix widget default layer colors (when palette = Default)
constexpr Col4f helix_default    = { 0.78f, 0.78f, 0.78f, 1.0f };
constexpr Col4f surface_default  = { 0.3f, 0.4f, 0.7f, 1.0f };
constexpr Col4f envelope_default = { 0.39f, 0.78f, 0.39f, 1.0f };

// potential overlay
constexpr Col4f potential_marginal = { 0.5f, 0.5f, 0.5f, 1.0f };

// absorb zone (GL)
constexpr Col4f absorb_gl_edge = { 0.1f, 0.1f, 0.8f, 1.0f };
constexpr Col4f absorb_gl_zero = { 0.1f, 0.1f, 0.8f, 0.0f };

// panel
constexpr Col4u panel_separator = { 40, 60, 80, 255 };

} // namespace colors
