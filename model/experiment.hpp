#pragma once

#include <vector>
#include <memory>
#include <chrono>

#include "setup.hpp"
#include "simulation.hpp"

class Experiment {
public:
	Setup setup{};

	// global clock
	double sim_time{};               // current simulation time (seconds)
	double sim_target{};             // target time to advance to
	double timescale{1e-15};         // sim seconds per wall second (default 1 fs/s)
	bool running{false};

	// advance all simulations to sim_target
	// advance simulations, but don't spend more than budget_ms on it.
	// all sims stay in lockstep — clock tracks the slowest.
	void advance(double wall_dt, double budget_ms = 12.0) {
		if(!running || simulations.empty()) return;
		sim_target += timescale * wall_dt;

		// per-sim budget, split evenly
		double per_sim_ms = budget_ms / simulations.size();
		double slowest = sim_target;

		for(auto &sim : simulations) {
			auto t0 = std::chrono::steady_clock::now();
			while(sim->time() < sim_target) {
				sim->step();
				auto elapsed = std::chrono::steady_clock::now() - t0;
				double ms = std::chrono::duration<double, std::milli>(elapsed).count();
				if(ms > per_sim_ms)
					break;
			}
			if(sim->time() < slowest)
				slowest = sim->time();
		}

		// clamp target to slowest — nobody runs ahead
		sim_target = slowest;
		sim_time = slowest;
	}

	// owned simulations
	std::vector<std::unique_ptr<Simulation>> simulations{};
};
