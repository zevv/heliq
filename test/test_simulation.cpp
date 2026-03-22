#include "/home/ico/external/doctest.h"
#include "simulation.hpp"
#include "loader.hpp"

TEST_CASE("simulation allocation from setup") {
	Setup setup{};
	REQUIRE(load_setup("test/test-1d-barrier.lua", setup));
	REQUIRE(setup.simulations.size() == 1);

	Simulation sim(setup.simulations[0], setup);

	CHECK(sim.name == "default");
	CHECK(sim.mode == SimMode::Joint);
	CHECK(sim.dt > 0);
	CHECK(sim.step_count == 0);
	CHECK(sim.time() == 0);

	SUBCASE("grid") {
		CHECK(sim.grid.rank == 1);
		CHECK(sim.grid.axes[0].points == 512);
		CHECK(sim.grid.total_points() == 512);
	}

	SUBCASE("arrays allocated") {
		REQUIRE(!sim.psi.empty());
		REQUIRE(!sim.potential.empty());
		REQUIRE(!sim.psi_initial.empty());
	}

	SUBCASE("potential sampled") {
		int mid = sim.grid.axes[0].points / 2;
		CHECK(sim.potential[mid].real() > 0);
		CHECK(sim.potential[0].real() == doctest::Approx(0));
	}

	SUBCASE("wavefunction sampled and normalized") {
		double prob = 0;
		double dv = sim.grid.axes[0].dx();
		for(size_t i = 0; i < sim.grid.total_points(); i++)
			prob += std::norm(sim.psi[i]) * dv;
		CHECK(prob == doctest::Approx(1.0).epsilon(0.01));

		for(size_t i = 0; i < sim.grid.total_points(); i++)
			CHECK(sim.psi[i] == sim.psi_initial[i]);
	}
}

TEST_CASE("simulation step evolves wavefunction") {
	Setup setup{};
	REQUIRE(load_setup("test/test-1d-barrier.lua", setup));

	Simulation sim(setup.simulations[0], setup);

	// step many times — enough for visible movement
	for(int i = 0; i < 10000; i++)
		sim.step();

	// check probability conservation
	double prob = sim.total_probability();
	CHECK(prob == doctest::Approx(1.0).epsilon(0.01));

	// check simulation time advanced
	CHECK(sim.time() > 0);
	CHECK(sim.step_count == 10000);
}

TEST_CASE("simulation with custom resolution") {
	Setup setup{};
	REQUIRE(load_setup("test/test-1d-barrier.lua", setup));

	SimConfig config{};
	config.name = "coarse";
	config.resolution = 128;
	config.dt = 1e-13;

	Simulation sim(config, setup);

	CHECK(sim.grid.axes[0].points == 128);
	CHECK(sim.grid.total_points() == 128);
	CHECK(sim.grid.axes[0].min == doctest::Approx(-5e-6));
	CHECK(sim.grid.axes[0].max == doctest::Approx(5e-6));
}
