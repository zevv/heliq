#pragma once

#include <assert.h>
#include <stddef.h>

constexpr int MAX_RANK = 8;

struct Axis {
	int points{};
	double min{};          // meters
	double max{};          // meters
	bool spatial{true};    // false for internal dof (spin)
	char label[8]{};       // e.g. "P1.x", "x"

	double dx() const { return (max - min) / points; }
};

struct Grid {
	int rank{};
	Axis axes[MAX_RANK]{};
	int stride[MAX_RANK]{};

	void compute_strides() {
		stride[rank - 1] = 1;
		for(int i = rank - 2; i >= 0; i--) {
			stride[i] = stride[i + 1] * axes[i + 1].points;
		}
	}

	size_t total_points() const {
		size_t n = 1;
		for(int i = 0; i < rank; i++) n *= axes[i].points;
		return n;
	}

	size_t linear_index(const int *coords) const {
		size_t idx = 0;
		for(int i = 0; i < rank; i++) {
			assert(coords[i] >= 0 && coords[i] < axes[i].points);
			idx += coords[i] * stride[i];
		}
		return idx;
	}

	void coords_from_index(size_t idx, int *coords) const {
		for(int d = 0; d < rank; d++) {
			coords[d] = (int)(idx / stride[d]);
			idx %= stride[d];
		}
	}

	// coordinate-aware iteration over all grid points.
	// callback receives (linear_index, coords[], positions[])
	template<typename F>
	void each(F &&fn) const {
		int coords[MAX_RANK] = {};
		double pos[MAX_RANK];
		size_t total = total_points();
		for(size_t idx = 0; idx < total; idx++) {
			for(int d = 0; d < rank; d++)
				pos[d] = axes[d].min + coords[d] * axes[d].dx();
			fn(idx, coords, pos);
			// increment coordinates, last axis fastest (row-major)
			for(int d = rank - 1; d >= 0; d--) {
				if(++coords[d] < axes[d].points) break;
				coords[d] = 0;
			}
		}
	}
};
