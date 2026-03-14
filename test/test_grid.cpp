#include "/home/ico/external/doctest.h"
#include "grid.hpp"

TEST_CASE("grid 1D") {
	Grid g{};
	g.rank = 1;
	g.axes[0] = { .points = 100, .min = -1.0, .max = 1.0 };
	g.compute_strides();

	CHECK(g.total_points() == 100);
	CHECK(g.stride[0] == 1);
	CHECK(g.axes[0].dx() == doctest::Approx(0.02));
}

TEST_CASE("grid 2D strides") {
	Grid g{};
	g.rank = 2;
	g.axes[0] = { .points = 64, .min = 0, .max = 1.0 };
	g.axes[1] = { .points = 128, .min = 0, .max = 1.0 };
	g.compute_strides();

	CHECK(g.total_points() == 64 * 128);
	CHECK(g.stride[0] == 128);
	CHECK(g.stride[1] == 1);

	int coords[] = {3, 7};
	CHECK(g.linear_index(coords) == 3 * 128 + 7);
}

TEST_CASE("grid each iterates all points") {
	Grid g{};
	g.rank = 2;
	g.axes[0] = { .points = 4, .min = 0, .max = 1.0 };
	g.axes[1] = { .points = 3, .min = 0, .max = 1.0 };
	g.compute_strides();

	size_t count = 0;
	g.each([&](size_t idx, const int *coords, const double *pos) {
		CHECK(idx == count);
		CHECK(g.linear_index(coords) == idx);
		count++;
	});
	CHECK(count == 12);
}
