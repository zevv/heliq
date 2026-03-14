
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
}


Simulation::~Simulation()
{
	fftw_free(psi[0]);
	fftw_free(psi[1]);
	fftw_free(potential);
	fftw_free(psi_initial);
}
