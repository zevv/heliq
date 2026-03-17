#pragma once

#include <complex>
#include <math.h>
#include "grid.hpp"

// Data sources for grid/surface visualization overlays

enum class DataSource {
	Off,
	PsiSq,     // |ψ|²
	PsiRe,     // Re(ψ)
	PsiIm,     // Im(ψ)
	PsiPhase,  // arg(ψ)
	Potential,  // Re(V)
	COUNT
};

static const char *datasource_names[] = {
	"off", "|psi|^2", "Re(psi)", "Im(psi)", "phase(psi)", "potential"
};

inline double sample_value(DataSource src, psi_t psi, psi_t pot) {
	switch(src) {
		case DataSource::PsiSq:    return std::norm(psi);
		case DataSource::PsiRe:    return psi.real();
		case DataSource::PsiIm:    return psi.imag();
		case DataSource::PsiPhase: return std::arg(psi);
		case DataSource::Potential: return pot.real();
		default: return 0;
	}
}

// Color palettes

enum class Palette {
	Flame,
	Gray,
	Rainbow,
	Zebra,
	Spatial,
	COUNT
};

static const char *palette_names[] = {
	"flame", "gray", "rainbow", "zebra", "spatial"
};

inline uint32_t palette_flame(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	int r = 255;
	int g = (int)(255 * fmin(1.0, t * 2.0));
	int b = (int)(255 * fmin(1.0, fmax(0.0, t * 2.0 - 1.0)));
	return (a << 24) | (b << 16) | (g << 8) | r;
}

inline uint32_t palette_gray(double v, double gamma) {
	double t = fmin(1.0, v);
	int a = (int)(255 * pow(t, gamma));
	return (a << 24) | (255 << 16) | (255 << 8) | 255;
}

// map 2D grid position to hue [0,1] using angle from center
inline double spatial_hue(int x, int y, int tw, int th) {
	double cx = (double)x / (tw - 1) - 0.5;
	double cy = (double)y / (th - 1) - 0.5;
	return fmod(atan2(cy, cx) / (2.0 * M_PI) + 1.0, 1.0);
}
