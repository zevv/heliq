#pragma once

#include <vector>
#include <string>

#include "grid.hpp"

// Output of Lua ingestion. Immutable after creation.
// Three arrays (psi, potential, mass) plus metadata.

enum class SimMode { Joint };

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
	int n_particles{};
	int dims_per_particle{};
	Axis domain[MAX_RANK]{};
	double mass[MAX_RANK]{};         // kg, per config-space axis
	std::vector<psi_t> psi_init;     // pre-sampled psi(pos), one per grid point
	std::vector<psi_t> potential;    // pre-sampled V(pos), one per grid point
	std::vector<SimConfig> simulations{};
	double timescale{1e-15};
	double default_timescale{1e-15};
	double default_dt{};
	bool absorbing_boundary{false};
	double absorb_width{0.02};
	double absorb_strength{};
};
