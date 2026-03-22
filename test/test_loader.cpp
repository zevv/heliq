#include "/home/ico/external/doctest.h"
#include "loader.hpp"

TEST_CASE("load 1d-barrier experiment") {
	Setup setup{};
	REQUIRE(load_setup("test/test-1d-barrier.lua", setup));

	CHECK(setup.spatial_dims == 1);
	CHECK(setup.n_particles == 1);

	SUBCASE("domain") {
		CHECK(setup.domain[0].points == 512);
		CHECK(setup.domain[0].min == doctest::Approx(-5e-6));
		CHECK(setup.domain[0].max == doctest::Approx(5e-6));
		CHECK(setup.domain[0].spatial == true);
	}

	SUBCASE("mass") {
		CHECK(setup.mass[0] == doctest::Approx(9.109e-31).epsilon(0.001));
	}

	SUBCASE("potential sampled") {
		CHECK(setup.potential.size() == 512);
		// potential should be nonzero somewhere (barrier region)
		bool has_nonzero = false;
		for(auto &v : setup.potential)
			if(v.real() > 0) has_nonzero = true;
		CHECK(has_nonzero);
	}

	SUBCASE("psi_init sampled") {
		CHECK(setup.psi_init.size() == 512);
		// psi should be nonzero somewhere
		bool has_nonzero = false;
		for(auto &v : setup.psi_init)
			if(std::norm(v) > 0) has_nonzero = true;
		CHECK(has_nonzero);
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
