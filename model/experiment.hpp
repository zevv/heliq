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
	double timescale{1e-15};         // sim seconds per wall second (default 1 fs/s)
	bool running{false};

	// advance simulations, but don't spend more than budget_ms on it.
	// all sims stay in lockstep — clock tracks the slowest.
	void advance(double wall_dt, double budget_ms = 12.0) {
		if(!running || simulations.empty()) return;

		// target is always relative to current sim_time, never accumulates debt
		double target = sim_time + timescale * wall_dt;

		// per-sim budget, split evenly
		double per_sim_ms = budget_ms / simulations.size();
		double slowest = target;

		for(auto &sim : simulations) {
			auto t0 = std::chrono::steady_clock::now();
			while(sim->time() < target) {
				sim->step();
				auto elapsed = std::chrono::steady_clock::now() - t0;
				double ms = std::chrono::duration<double, std::milli>(elapsed).count();
				if(ms > per_sim_ms)
					break;
			}
			if(sim->time() < slowest)
				slowest = sim->time();
		}

		sim_time = slowest;
	}

	// owned simulations
	std::vector<std::unique_ptr<Simulation>> simulations{};
};
