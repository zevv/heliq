
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

	size_t n = grid.total_points();
	size_t bytes = n * sizeof(std::complex<double>);

	// CPU-side arrays for widget display and setup data
	psi[0]          = (std::complex<double> *)fftw_malloc(bytes);
	psi[1]          = (std::complex<double> *)fftw_malloc(bytes);
	potential       = (std::complex<double> *)fftw_malloc(bytes);
	psi_initial     = (std::complex<double> *)fftw_malloc(bytes);
	m_potential_phase = (std::complex<double> *)fftw_malloc(bytes);
	m_kinetic_phase   = (std::complex<double> *)fftw_malloc(bytes);

	std::fill_n(psi[0],            n, std::complex<double>(0));
	std::fill_n(psi[1],            n, std::complex<double>(0));
	std::fill_n(potential,         n, std::complex<double>(0));
	std::fill_n(psi_initial,       n, std::complex<double>(0));
	std::fill_n(m_potential_phase,  n, std::complex<double>(0));
	std::fill_n(m_kinetic_phase,    n, std::complex<double>(0));

	// sample potentials and wavefunction into CPU arrays
	sample_potential(setup);
	sample_wavefunction(setup);

	// precompute phase factors
	precompute_phases();

	// create solver and upload data
	m_solver = Solver::create(grid);
	upload_phases();
	m_solver->write_psi(psi_initial);

	// copy initial state into display buffers
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
		m_potential_phase[i] = amp * std::complex<double>(cos(phase), sin(phase));
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
		m_kinetic_phase[idx] = std::complex<double>(cos(-phase), sin(-phase));
		if(phase > max_kinetic_phase) max_kinetic_phase = phase;
	});
}


void Simulation::upload_phases()
{
	m_solver->set_phases(m_potential_phase, m_kinetic_phase);
}


double Simulation::total_probability()
{
	double prob = 0;
	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	auto *p = psi_front();
	size_t n = grid.total_points();
	for(size_t i = 0; i < n; i++)
		prob += std::norm(p[i]) * dv;
	return prob;
}


void Simulation::set_dt(double new_dt)
{
	if(new_dt == dt) return;
	dt = new_dt;
	precompute_phases();
	upload_phases();
}


void Simulation::step()
{
	step_compute();
	sync();
}


void Simulation::step_compute()
{
	m_solver->step();
	step_count++;
	sim_time += dt;
}


void Simulation::flush()
{
	m_solver->flush();
}


void Simulation::sync()
{
	int back = 1 - front.load();
	m_solver->read_psi(psi[back]);
	front.store(back);
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
	// solver destroyed via unique_ptr before we free arrays
	m_solver.reset();
	fftw_free(psi[0]);
	fftw_free(psi[1]);
	fftw_free(potential);
	fftw_free(psi_initial);
	fftw_free(m_potential_phase);
	fftw_free(m_kinetic_phase);
}
