
#include <stdio.h>
#include <chrono>
#include <math.h>

#include "experiment.hpp"
#include "loader.hpp"


bool Experiment::load(const std::string& path)
{
	simulations.clear();
	setup = Setup{};

	if(!load_setup(path.c_str(), setup, true)) {
		fprintf(stderr, "failed to load experiment from %s\n", path.c_str());
		return false;
	}

	fprintf(stderr, "loaded: %dD, %zu particles, %zu potentials, %zu sims\n",
		setup.spatial_dims, setup.particles.size(),
		setup.potentials.size(), setup.simulations.size());

	timescale = setup.timescale;

	for(auto &sc : setup.simulations)
		simulations.push_back(std::make_unique<Simulation>(sc, setup));

	sim_time = 0;
	batch_size = 4;
	running = false;

	return true;
}


void Experiment::advance(double wall_dt, double budget_ms)
{
	if(!running || simulations.empty()) return;

	double target = sim_time + timescale * wall_dt;
	bool forward = timescale >= 0;

	// ensure sim dt sign matches timescale direction
	for(auto &sim : simulations) {
		double desired_dt = (forward ? 1.0 : -1.0) * fabs(sim->dt);
		if(sim->dt != desired_dt)
			sim->set_dt(desired_dt);
	}

	// ensure at least one step per frame
	for(auto &sim : simulations) {
		bool past = forward ? sim->time() >= target : sim->time() <= target;
		if(past)
			target = sim->time() + sim->dt;
	}

	auto reached = [&](double t) {
		return forward ? t >= target : t <= target;
	};

	double per_sim_ms = budget_ms / simulations.size();
	double slowest = target;

	for(auto &sim : simulations) {
		auto t0 = std::chrono::steady_clock::now();
		while(!reached(sim->time())) {
			int batch = batch_size;
			for(int i = 0; i < batch && !reached(sim->time()); i++)
				sim->step_compute();
			sim->flush();

			auto elapsed = std::chrono::steady_clock::now() - t0;
			double ms = std::chrono::duration<double, std::milli>(elapsed).count();
			if(ms > per_sim_ms)
				break;

			if(ms < per_sim_ms * 0.5 && batch_size < 1024)
				batch_size = batch_size * 2;
			else if(ms > per_sim_ms * 0.9 && batch_size > 1)
				batch_size = batch_size / 2;
		}
		sim->flush();
		bool behind = forward ? sim->time() < slowest : sim->time() > slowest;
		if(behind)
			slowest = sim->time();
	}

	sim_time = slowest;
}
