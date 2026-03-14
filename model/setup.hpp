#pragma once

#include <complex>
#include <vector>

#include "grid.hpp"

// Output of Lua ingestion. Immutable after creation.
// Pure data — no simulation logic, no UI coupling.

struct Particle {
	double mass{};                   // kg
	double position[MAX_RANK]{};     // meters, per spatial dim
	double momentum[MAX_RANK]{};     // kg·m/s
	double width{};                  // meters, Gaussian spread
};

struct Potential {
	enum Type { Barrier, Well, Harmonic, Absorbing };
	Type type{};
	double bounds_min[MAX_RANK]{};   // meters
	double bounds_max[MAX_RANK]{};   // meters
	std::complex<double> height{};   // eV (imaginary part for absorbing)
};

struct Setup {
	int spatial_dims{};
	Axis domain[MAX_RANK]{};
	std::vector<Particle> particles{};
	std::vector<Potential> potentials{};  // includes absorbing boundaries
};
