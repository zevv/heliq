#pragma once

#include <vector>
#include <memory>
#include <string>

#include "setup.hpp"
#include "simulation.hpp"

class Experiment {
public:
	Setup setup{};

	// global clock
	double sim_time{};               // current simulation time (seconds)
	double timescale{1e-15};         // sim seconds per wall second (default 1 fs/s)
	bool running{false};

	// load from pre-parsed Setup (no Lua dependency)
	bool load(Setup new_setup, bool reset = false);

	// advance simulations within budget
	void advance(double wall_dt, double budget_ms = 12.0);

	// owned simulations
	std::vector<std::unique_ptr<Simulation>> simulations{};

private:
	int batch_size{4};  // adaptive, grows/shrinks per frame
};
