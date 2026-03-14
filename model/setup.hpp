#pragma once

#include <complex>
#include <vector>
#include <string>

#include "grid.hpp"

// Output of Lua ingestion. Immutable after creation.
// Pure data — no simulation logic, no UI coupling.

struct Particle {
	double mass{};                   // kg
	double charge{};                 // C
	double position[MAX_RANK]{};     // meters, per spatial dim
	double momentum[MAX_RANK]{};     // kg·m/s
	double width{};                  // meters, Gaussian spread
};

struct Potential {
	enum Type { Barrier, Well, Harmonic, Absorbing };
	Type type{};
	double from[MAX_RANK]{};         // meters
	double to[MAX_RANK]{};           // meters
	double center[MAX_RANK]{};       // meters (harmonic)
	double height{};                 // eV (barrier/absorbing)
	double depth{};                  // eV (well, stored as positive)
	double k{};                      // N/m (harmonic spring constant)
};

enum class SimMode { Joint, Factored };

struct SimConfig {
	std::string name{};
	SimMode mode{SimMode::Joint};
	int resolution{};                // 0 = use domain resolution
	double dt{};                     // seconds
};

struct Setup {
	int spatial_dims{};
	Axis domain[MAX_RANK]{};
	std::vector<Particle> particles{};
	std::vector<Potential> potentials{};
	std::vector<SimConfig> simulations{};
	double timescale{1e-15};
};
