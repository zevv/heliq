#include "/home/ico/external/doctest.h"
#include "loader.hpp"

TEST_CASE("load 1d-barrier experiment") {
	Setup setup{};
	REQUIRE(load_setup("experiments/1d-barrier.lua", setup));

	CHECK(setup.spatial_dims == 1);

	SUBCASE("domain") {
		CHECK(setup.domain[0].points == 512);
		CHECK(setup.domain[0].min == doctest::Approx(-5e-6));
		CHECK(setup.domain[0].max == doctest::Approx(5e-6));
		CHECK(setup.domain[0].spatial == true);
	}

	SUBCASE("particles") {
		REQUIRE(setup.particles.size() == 1);
		auto &p = setup.particles[0];
		CHECK(p.mass == doctest::Approx(9.109e-31).epsilon(0.001));
		CHECK(p.charge == doctest::Approx(-1.602e-19).epsilon(0.001));
		CHECK(p.position[0] == doctest::Approx(-2e-6));
		CHECK(p.momentum[0] > 0);
		CHECK(p.width == doctest::Approx(0.5e-6));
	}

	SUBCASE("potentials") {
		REQUIRE(setup.potentials.size() == 1);
		auto &pot = setup.potentials[0];
		CHECK(pot.type == Potential::Barrier);
		CHECK(pot.height > 0);
	}

	SUBCASE("simulations") {
		REQUIRE(setup.simulations.size() == 1);
		auto &sim = setup.simulations[0];
		CHECK(sim.name == "default");
		CHECK(sim.mode == SimMode::Joint);
		CHECK(sim.dt > 0);
	}
}

TEST_CASE("load nonexistent script fails") {
	Setup setup{};
	CHECK_FALSE(load_setup("nonexistent.lua", setup));
}
