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
	enum class Type { Barrier, Well, Harmonic, Absorbing };
	Type type{};
	double from[MAX_RANK]{};         // meters
	double to[MAX_RANK]{};           // meters
	double center[MAX_RANK]{};       // meters (harmonic)
	double height{};                 // J (barrier/absorbing)
	double depth{};                  // J (well, stored as positive)
	double k{};                      // N/m (harmonic spring constant)
};

struct Interaction {
	enum class Type { Coulomb, Contact };
	Type type{};
	int particle_a{};                // 0-indexed particle indices
	int particle_b{};
	double softening{};              // meters, regularization for 1/r (Coulomb)
	double strength{1.0};            // Contact: Joules; Coulomb: multiplier (1.0 = real)
	double width{};                  // meters, barrier width (Contact)
};

enum class SimMode { Joint, Factored };

struct SimConfig {
	std::string name{};
	SimMode mode{SimMode::Joint};
	int resolution{};                // 0 = use domain resolution
	double dt{};                     // seconds
};

struct Setup {
	std::string title{};
	std::string description{};
	int spatial_dims{};
	Axis domain[MAX_RANK]{};
	std::vector<Particle> particles{};
	std::vector<Potential> potentials{};
	std::vector<Interaction> interactions{};
	std::vector<SimConfig> simulations{};
	double timescale{1e-15};         // current timescale
	double default_timescale{1e-15}; // auto-computed, for 'A' reset
	double default_dt{};             // auto-computed, for 'A' reset
	bool absorbing_boundary{false};
	double absorb_width{0.02};     // fraction of domain width per side
	double absorb_strength{};      // 0 = auto-compute from potential/grid
};
