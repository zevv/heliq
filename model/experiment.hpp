#pragma once

#include <vector>
#include <memory>

#include "setup.hpp"
#include "simulation.hpp"

class Experiment {
public:
	Setup setup{};

	// global clock
	double sim_time{};               // current simulation time (seconds)
	double timescale{1e-15};         // sim seconds per wall second (default 1 fs/s)
	bool running{false};

	// owned simulations
	std::vector<std::unique_ptr<Simulation>> simulations{};
};
