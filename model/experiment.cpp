
#include <chrono>
#include <math.h>

#include "experiment.hpp"
#include "log.hpp"


bool Experiment::load(Setup new_setup, bool reset)
{
	// preserve user-adjusted timescale and dt across reloads (R key),
	// but not when switching to a different experiment (reset=true)
	bool is_reload = !reset && !simulations.empty();
	double prev_timescale = timescale;
	double prev_dt = is_reload ? simulations[0]->dt : 0;

	simulations.clear();
	setup = std::move(new_setup);

	linf("loaded: %dD, %zu particles, %zu potentials, %zu sims",
		setup.spatial_dims, setup.particles.size(),
		setup.potentials.size(), setup.simulations.size());

	if(is_reload) {
		timescale = prev_timescale;
	} else {
		timescale = setup.timescale;
	}

	for(auto &sc : setup.simulations)
		simulations.push_back(std::make_unique<Simulation>(sc, setup));

	if(is_reload && prev_dt != 0)
		for(auto &sim : simulations)
			sim->set_dt(prev_dt);

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
			for(int i = 0; i < batch_size && !reached(sim->time()); i++)
				sim->step_compute();
			sim->flush();

			auto elapsed = std::chrono::steady_clock::now() - t0;
			double ms = std::chrono::duration<double, std::milli>(elapsed).count();
			if(ms > per_sim_ms)
				break;
		}
		sim->flush();

		// adapt batch size based on how this frame went
		auto elapsed = std::chrono::steady_clock::now() - t0;
		double ms = std::chrono::duration<double, std::milli>(elapsed).count();
		if(ms < per_sim_ms * 0.5 && batch_size < 1024)
			batch_size *= 2;
		else if(ms > per_sim_ms * 0.9 && batch_size > 1)
			batch_size /= 2;

		bool behind = forward ? sim->time() < slowest : sim->time() > slowest;
		if(behind)
			slowest = sim->time();
	}

	sim_time = slowest;
}
