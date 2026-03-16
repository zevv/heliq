#pragma once

#include "grid.hpp"
#include <math.h>

// Maps between physical particles/dimensions and config-space axes.
// For N particles in D spatial dimensions, the config space has N*D axes.
// Axis index = particle * spatial_dims + dim.
//
// Single particle: axes map directly to spatial dimensions.
// Multi-particle joint: axes are grouped by particle.

struct ConfigSpace {
	int n_particles{};
	int spatial_dims{};  // per particle (1, 2, or 3)

	// particle P, spatial dimension D → config space axis
	int axis(int particle, int dim) const {
		return particle * spatial_dims + dim;
	}

	// config space axis → which particle
	int particle_of(int ax) const {
		return (n_particles > 1) ? ax / spatial_dims : 0;
	}

	// config space axis → which spatial dimension of that particle
	int dim_of(int ax) const {
		return (n_particles > 1) ? ax % spatial_dims : ax;
	}

	// Euclidean distance² between particles pa and pb
	// pos[] is indexed by config space axis
	double distance_sq(int pa, int pb, const double *pos) const {
		double d2 = 0;
		for(int d = 0; d < spatial_dims; d++) {
			double dx = pos[axis(pa, d)] - pos[axis(pb, d)];
			d2 += dx * dx;
		}
		return d2;
	}

	// Euclidean distance between particles pa and pb
	double distance(int pa, int pb, const double *pos) const {
		return sqrt(distance_sq(pa, pb, pos));
	}

	// total config space rank
	int rank() const { return n_particles * spatial_dims; }
};
