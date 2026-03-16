// Headless test: load a 4D experiment, run N steps, check probability conservation.
// Usage: ./test_solver_4d experiments/2p2d-eraser.lua [nsteps]

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex>
#include <memory>

#include "setup.hpp"
#include "loader.hpp"
#include "simulation.hpp"

int main(int argc, char **argv)
{
	if(argc < 2) {
		fprintf(stderr, "usage: %s <script.lua> [nsteps]\n", argv[0]);
		return 1;
	}

	const char *script = argv[1];
	int nsteps = (argc > 2) ? atoi(argv[2]) : 10;

	// load
	Setup setup;
	if(!load_setup(script, setup, true)) {
		fprintf(stderr, "failed to load %s\n", script);
		return 1;
	}

	if(setup.simulations.empty()) {
		fprintf(stderr, "no simulations defined\n");
		return 1;
	}

	// create simulation
	auto sim = std::make_unique<Simulation>(setup.simulations[0], setup);
	fprintf(stderr, "grid rank=%d, total=%zu points\n", sim->grid.rank, sim->grid.total_points());

	// initial probability
	double p0 = sim->total_probability();
	fprintf(stderr, "initial probability: %.10f\n", p0);

	// run steps
	for(int i = 0; i < nsteps; i++) {
		sim->step();
		if((i + 1) % 5 == 0 || i == nsteps - 1) {
			sim->sync();
			double p = sim->total_probability();
			double drift = fabs(p - p0) / p0;
			fprintf(stderr, "step %d: P=%.10f  drift=%.2e\n", i + 1, p, drift);
		}
	}
	sim->sync();

	double pf = sim->total_probability();
	double drift = fabs(pf - p0) / p0;
	fprintf(stderr, "\nfinal probability: %.10f (drift %.2e)\n", pf, drift);
	fprintf(stderr, "%s\n", drift < 0.01 ? "PASS" : "FAIL");

	return (drift < 0.01) ? 0 : 1;
}
