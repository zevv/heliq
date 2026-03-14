
#include <fftw3.h>
#include <algorithm>
#include <math.h>

#include "simulation.hpp"
#include "constants.hpp"


Simulation::Simulation(const SimConfig &config, const Setup &setup)
{
	name = config.name;
	mode = config.mode;
	dt = config.dt;
	mass = setup.particles.empty() ? 1.0 : setup.particles[0].mass;

	// build grid from setup domain
	grid.rank = setup.spatial_dims;
	for(int i = 0; i < grid.rank; i++) {
		grid.axes[i] = setup.domain[i];
		if(config.resolution > 0)
			grid.axes[i].points = config.resolution;
	}
	grid.compute_strides();

	// allocate arrays (fftw_malloc for SIMD alignment)
	size_t n = grid.total_points();
	size_t bytes = n * sizeof(std::complex<double>);

	psi[0]          = (std::complex<double> *)fftw_malloc(bytes);
	psi[1]          = (std::complex<double> *)fftw_malloc(bytes);
	potential       = (std::complex<double> *)fftw_malloc(bytes);
	potential_phase = (std::complex<double> *)fftw_malloc(bytes);
	kinetic_phase   = (std::complex<double> *)fftw_malloc(bytes);
	psi_initial     = (std::complex<double> *)fftw_malloc(bytes);

	std::fill_n(psi[0],          n, std::complex<double>(0));
	std::fill_n(psi[1],          n, std::complex<double>(0));
	std::fill_n(potential,       n, std::complex<double>(0));
	std::fill_n(potential_phase, n, std::complex<double>(0));
	std::fill_n(kinetic_phase,   n, std::complex<double>(0));
	std::fill_n(psi_initial,     n, std::complex<double>(0));

	// sample potentials and wavefunction
	sample_potential(setup);
	sample_wavefunction(setup);

	// precompute phase factors for split-step
	precompute_phases();

	// create FFTW plans
	// FFTW dims array: rank dimensions
	int dims[MAX_RANK];
	for(int i = 0; i < grid.rank; i++)
		dims[i] = grid.axes[i].points;

	fft_forward = fftw_plan_dft(grid.rank, dims,
		(fftw_complex *)psi[0], (fftw_complex *)psi[0],
		FFTW_FORWARD, FFTW_MEASURE);

	fft_inverse = fftw_plan_dft(grid.rank, dims,
		(fftw_complex *)psi[0], (fftw_complex *)psi[0],
		FFTW_BACKWARD, FFTW_MEASURE);

	// FFTW_MEASURE may have trashed psi[0], restore it
	std::copy_n(psi_initial, n, psi[0]);
	std::copy_n(psi_initial, n, psi[1]);
}


void Simulation::precompute_phases()
{
	size_t n = grid.total_points();

	// potential phase: exp(-i V dt / (2 hbar))
	max_potential_phase = 0;
	for(size_t i = 0; i < n; i++) {
		double vr = potential[i].real();
		double vi = potential[i].imag();
		double phase = -vr * dt / (2.0 * hbar);
		double decay = -vi * dt / (2.0 * hbar);
		double amp = exp(decay);
		potential_phase[i] = amp * std::complex<double>(cos(phase), sin(phase));
		if(fabs(phase) > max_potential_phase) max_potential_phase = fabs(phase);
	}

	// kinetic phase: exp(-i hbar k^2 dt / (2m))
	max_kinetic_phase = 0;
	grid.each([&](size_t idx, const int *coords, const double *pos) {
		double k2 = 0;
		for(int d = 0; d < grid.rank; d++) {
			int ni = grid.axes[d].points;
			double L = grid.axes[d].max - grid.axes[d].min;
			double dk = 2.0 * M_PI / L;
			double ki = (coords[d] < ni/2) ? coords[d] * dk : (coords[d] - ni) * dk;
			k2 += ki * ki;
		}
		double phase = hbar * k2 * dt / (2.0 * mass);
		kinetic_phase[idx] = std::complex<double>(cos(-phase), sin(-phase));
		if(phase > max_kinetic_phase) max_kinetic_phase = phase;
	});
}


double Simulation::total_probability()
{
	double prob = 0;
	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	auto *psi = psi_front();
	size_t n = grid.total_points();
	for(size_t i = 0; i < n; i++)
		prob += std::norm(psi[i]) * dv;
	return prob;
}


void Simulation::set_dt(double new_dt)
{
	if(new_dt == dt) return;
	dt = new_dt;
	precompute_phases();
}


void Simulation::step()
{
	size_t n = grid.total_points();
	int back = 1 - front.load();
	auto *buf = psi[back];

	// copy front to back (we work on back)
	std::copy_n(psi[front.load()], n, buf);

	// 1. half-step potential
	for(size_t i = 0; i < n; i++)
		buf[i] *= potential_phase[i];

	// 2. FFT forward
	fftw_execute_dft((fftw_plan)fft_forward, (fftw_complex *)buf, (fftw_complex *)buf);

	// 3. full-step kinetic (with FFTW normalization: divide by n on inverse)
	for(size_t i = 0; i < n; i++)
		buf[i] *= kinetic_phase[i];

	// 4. FFT inverse
	fftw_execute_dft((fftw_plan)fft_inverse, (fftw_complex *)buf, (fftw_complex *)buf);

	// FFTW inverse doesn't normalize, divide by n
	double inv_n = 1.0 / (double)n;
	for(size_t i = 0; i < n; i++)
		buf[i] *= inv_n;

	// 5. half-step potential
	for(size_t i = 0; i < n; i++)
		buf[i] *= potential_phase[i];

	// swap front
	front.store(back);
	step_count++;
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

	auto &p = setup.particles[0];
	double norm = 0;

	grid.each([&](size_t idx, const int *coords, const double *pos) {
		double envelope = 0;
		double phase = 0;
		for(int d = 0; d < grid.rank; d++) {
			double dx = pos[d] - p.position[d];
			envelope += dx * dx / (4.0 * p.width * p.width);
			phase += p.momentum[d] * pos[d] / hbar;
		}
		double amp = exp(-envelope);
		psi[0][idx] = amp * std::complex<double>(cos(phase), sin(phase));
		norm += amp * amp;
	});

	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	norm *= dv;
	double scale = 1.0 / sqrt(norm);

	size_t n = grid.total_points();
	for(size_t i = 0; i < n; i++)
		psi[0][i] *= scale;

	std::copy_n(psi[0], n, psi_initial);
	std::copy_n(psi[0], n, psi[1]);
}


Simulation::~Simulation()
{
	if(fft_forward) fftw_destroy_plan((fftw_plan)fft_forward);
	if(fft_inverse) fftw_destroy_plan((fftw_plan)fft_inverse);
	fftw_free(psi[0]);
	fftw_free(psi[1]);
	fftw_free(potential);
	fftw_free(potential_phase);
	fftw_free(kinetic_phase);
	fftw_free(psi_initial);
}
