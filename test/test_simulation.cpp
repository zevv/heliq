#include "/home/ico/external/doctest.h"
#include "simulation.hpp"
#include "loader.hpp"

TEST_CASE("simulation allocation from setup") {
	Setup setup{};
	REQUIRE(load_setup("experiments/barrier-1d.lua", setup));
	REQUIRE(setup.simulations.size() == 1);

	Simulation sim(setup.simulations[0], setup);

	CHECK(sim.name == "default");
	CHECK(sim.mode == SimMode::Joint);
	CHECK(sim.dt > 0);
	CHECK(sim.step_count == 0);
	CHECK(sim.time() == 0);

	SUBCASE("grid") {
		CHECK(sim.grid.rank == 1);
		CHECK(sim.grid.axes[0].points == 1024);
		CHECK(sim.grid.total_points() == 1024);
	}

	SUBCASE("arrays allocated") {
		REQUIRE(sim.psi[0] != nullptr);
		REQUIRE(sim.psi[1] != nullptr);
		REQUIRE(sim.potential != nullptr);
		REQUIRE(sim.psi_initial != nullptr);
	}

	SUBCASE("potential sampled") {
		// barrier is at [-50nm, 50nm], height = 5eV
		// check a point inside the barrier has nonzero potential
		int mid = sim.grid.axes[0].points / 2; // center of domain = 0
		CHECK(sim.potential[mid].real() > 0);

		// check a point far from barrier has zero potential
		CHECK(sim.potential[0].real() == doctest::Approx(0));
	}

	SUBCASE("wavefunction sampled and normalized") {
		// total probability should be 1
		double prob = 0;
		double dv = sim.grid.axes[0].dx();
		for(size_t i = 0; i < sim.grid.total_points(); i++)
			prob += std::norm(sim.psi[0][i]) * dv;
		CHECK(prob == doctest::Approx(1.0).epsilon(0.01));

		// psi and psi_initial should match
		for(size_t i = 0; i < sim.grid.total_points(); i++)
			CHECK(sim.psi[0][i] == sim.psi_initial[i]);
	}

	SUBCASE("front buffer") {
		CHECK(sim.psi_front() == sim.psi[0]);
	}
}

TEST_CASE("simulation with custom resolution") {
	Setup setup{};
	REQUIRE(load_setup("experiments/barrier-1d.lua", setup));

	SimConfig config{};
	config.name = "coarse";
	config.resolution = 256;
	config.dt = 1e-16;

	Simulation sim(config, setup);

	CHECK(sim.grid.axes[0].points == 256);
	CHECK(sim.grid.total_points() == 256);
	// domain extents preserved
	CHECK(sim.grid.axes[0].min == doctest::Approx(-5e-6));
	CHECK(sim.grid.axes[0].max == doctest::Approx(5e-6));
}
