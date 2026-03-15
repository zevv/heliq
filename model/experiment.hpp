#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <string>

#include "setup.hpp"
#include "simulation.hpp"
#include "loader.hpp"

class Experiment {
public:
	Setup setup{};

	// global clock
	double sim_time{};               // current simulation time (seconds)
	double timescale{1e-15};         // sim seconds per wall second (default 1 fs/s)
	bool running{false};

	// reload experiment from script
	bool load(const std::string& path) {
		// clear existing state
		simulations.clear();
		setup = Setup{};

		// load setup from script
		if(!load_setup(path.c_str(), setup, true)) {
			fprintf(stderr, "failed to load experiment from %s\n", path.c_str());
			return false;
		}

		fprintf(stderr, "loaded: %dD, %zu particles, %zu potentials, %zu sims\n",
			setup.spatial_dims, setup.particles.size(), setup.potentials.size(), setup.simulations.size());

		// apply timescale from setup
		timescale = setup.timescale;

		// create simulation instances
		for(auto &sc : setup.simulations) {
			simulations.push_back(std::make_unique<Simulation>(sc, setup));
		}

		// reset state
		sim_time = 0;
		batch_size = 4;
		running = false;

		return true;
	}

	// advance simulations, but don't spend more than budget_ms on it.
	// all sims stay in lockstep — clock tracks the slowest.
	void advance(double wall_dt, double budget_ms = 12.0) {
		if(!running || simulations.empty()) return;

		// target is always relative to current sim_time, never accumulates debt
		// timescale sign determines forward/reverse
		double target = sim_time + timescale * wall_dt;
		bool forward = timescale >= 0;

		// ensure sim dt sign matches timescale direction
		for(auto &sim : simulations) {
			double desired_dt = (forward ? 1.0 : -1.0) * fabs(sim->dt);
			if(sim->dt != desired_dt)
				sim->set_dt(desired_dt);
		}

		// ensure at least one step per frame so changing dt doesn't stall
		for(auto &sim : simulations) {
			bool past = forward ? sim->time() >= target : sim->time() <= target;
			if(past)
				target = sim->time() + sim->dt;
		}

		auto reached = [&](double t) {
			return forward ? t >= target : t <= target;
		};

		// per-sim budget, split evenly
		double per_sim_ms = budget_ms / simulations.size();
		double slowest = target;

		for(auto &sim : simulations) {
			auto t0 = std::chrono::steady_clock::now();
			while(!reached(sim->time())) {
				// enqueue a batch, then flush once to measure real GPU time
				int batch = batch_size;
				for(int i = 0; i < batch && !reached(sim->time()); i++)
					sim->step_compute();
				sim->flush();

				auto elapsed = std::chrono::steady_clock::now() - t0;
				double ms = std::chrono::duration<double, std::milli>(elapsed).count();
				if(ms > per_sim_ms)
					break;

				// adapt: if we used less than half the budget, double the batch
				// if we overshot, halve it
				if(ms < per_sim_ms * 0.5 && batch_size < 1024)
					batch_size = batch_size * 2;
				else if(ms > per_sim_ms * 0.9 && batch_size > 1)
					batch_size = batch_size / 2;
			}
			sim->sync();
			bool behind = forward ? sim->time() < slowest : sim->time() > slowest;
			if(behind)
				slowest = sim->time();
		}

		sim_time = slowest;
	}

	int batch_size{4};  // adaptive, grows/shrinks per frame

	// owned simulations
	std::vector<std::unique_ptr<Simulation>> simulations{};
};
