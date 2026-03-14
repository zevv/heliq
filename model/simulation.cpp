
#include <fftw3.h>
#include <algorithm>

#include "simulation.hpp"


Simulation::Simulation(const SimConfig &config, const Setup &setup)
{
	name = config.name;
	mode = config.mode;
	dt = config.dt;

	// build grid from setup domain
	grid.rank = setup.spatial_dims;
	for(int i = 0; i < grid.rank; i++) {
		grid.axes[i] = setup.domain[i];
		// override resolution if sim config specifies one
		if(config.resolution > 0)
			grid.axes[i].points = config.resolution;
	}
	grid.compute_strides();

	// allocate arrays (fftw_malloc for SIMD alignment)
	size_t n = grid.total_points();
	size_t bytes = n * sizeof(std::complex<double>);

	psi[0]      = (std::complex<double> *)fftw_malloc(bytes);
	psi[1]      = (std::complex<double> *)fftw_malloc(bytes);
	potential   = (std::complex<double> *)fftw_malloc(bytes);
	psi_initial = (std::complex<double> *)fftw_malloc(bytes);

	// zero everything
	std::fill_n(psi[0],      n, std::complex<double>(0));
	std::fill_n(psi[1],      n, std::complex<double>(0));
	std::fill_n(potential,    n, std::complex<double>(0));
	std::fill_n(psi_initial,  n, std::complex<double>(0));

	// sample potentials onto grid
	sample_potential(setup);

	// sample initial wavefunction
	sample_wavefunction(setup);
}


static bool inside_bounds(const double *pos, const double *from, const double *to, int rank)
{
	for(int d = 0; d < rank; d++) {
		if(pos[d] < from[d] || pos[d] > to[d])
			return false;
	}
	return true;
}


void Simulation::sample_potential(const Setup &setup)
{
	grid.each([&](size_t idx, const int *coords, const double *pos) {
		std::complex<double> v(0, 0);
		for(auto &pot : setup.potentials) {
			switch(pot.type) {
				case Potential::Barrier:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v += pot.height;
					break;
				case Potential::Well:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v -= pot.depth;
					break;
				case Potential::Harmonic: {
					double r2 = 0;
					for(int d = 0; d < grid.rank; d++) {
						double dx = pos[d] - pot.center[d];
						r2 += dx * dx;
					}
					v += 0.5 * pot.k * r2;
					break;
				}
				case Potential::Absorbing:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v += std::complex<double>(0, pot.height);
					break;
			}
		}
		potential[idx] = v;
	});
}


void Simulation::sample_wavefunction(const Setup &setup)
{
	if(setup.particles.empty()) return;

	// for v1: single particle Gaussian wave packet
	auto &p = setup.particles[0];
	double norm = 0;

	grid.each([&](size_t idx, const int *coords, const double *pos) {
		// Gaussian envelope: exp(-(x-x0)²/(4σ²))
		// with momentum: * exp(i p0 x / hbar)
		double envelope = 0;
		double phase = 0;
		for(int d = 0; d < grid.rank; d++) {
			double dx = pos[d] - p.position[d];
			envelope += dx * dx / (4.0 * p.width * p.width);
			phase += p.momentum[d] * pos[d] / 1.054571817e-34; // hbar
		}
		double amp = exp(-envelope);
		psi[0][idx] = amp * std::complex<double>(cos(phase), sin(phase));
		norm += amp * amp;
	});

	// normalize: total probability = 1
	// integral of |ψ|² dV = 1, dV = product of dx's
	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	norm *= dv;
	double scale = 1.0 / sqrt(norm);

	size_t n = grid.total_points();
	for(size_t i = 0; i < n; i++)
		psi[0][i] *= scale;

	// copy to initial state snapshot and back buffer
	std::copy_n(psi[0], n, psi_initial);
	std::copy_n(psi[0], n, psi[1]);
}


Simulation::~Simulation()
{
	fftw_free(psi[0]);
	fftw_free(psi[1]);
	fftw_free(potential);
	fftw_free(psi_initial);
}
