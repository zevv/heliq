
#include <fftw3.h>
#include <algorithm>
#include <math.h>
#include <random>

#include "simulation.hpp"
#include "constants.hpp"

Simulation::Simulation(const SimConfig &config, const Setup &setup)
{
	name = config.name;
	mode = config.mode;
	dt = config.dt;

	// build config space mapping
	int np = (int)setup.particles.size();
	int ndomain = (int)setup.spatial_dims;  // number of domain axes from lua
	cs.n_particles = np;
	cs.spatial_dims = (np > 1) ? ndomain / np : ndomain;

	// build grid from setup domain
	grid.rank = ndomain;
	for(int i = 0; i < grid.rank; i++) {
		grid.axes[i] = setup.domain[i];
		if(config.resolution > 0)
			grid.axes[i].points = config.resolution;
	}
	grid.compute_strides();

	// assign per-axis mass using config space mapping
	for(int i = 0; i < MAX_RANK; i++) mass[i] = 1.0;
	for(int p = 0; p < np; p++) {
		for(int d = 0; d < cs.spatial_dims; d++) {
			int ax = cs.axis(p, d);
			if(ax < grid.rank)
				mass[ax] = setup.particles[p].mass;
		}
	}

	// generate axis labels from config space mapping
	cs.label_axes(grid);

	size_t n = grid.total_points();

	// CPU-side arrays
	psi[0].resize(n);
	psi[1].resize(n);
	potential.resize(n);
	psi_initial.resize(n);
	m_potential_phase.resize(n);
	m_kinetic_phase.resize(n);

	// sample potentials and wavefunction into CPU arrays
	sample_potential(setup);
	sample_wavefunction(setup);

	// compute spatial aliasing diagnostic: k / k_nyquist per axis
	for(int d = 0; d < grid.rank; d++) {
		int pi = cs.particle_of(d);
		int di = cs.dim_of(d);
		double p_val = (pi < (int)setup.particles.size())
			? fabs(setup.particles[pi].momentum[di]) : 0;
		double k = p_val / hbar;
		double k_nyq = M_PI / grid.axes[d].dx();
		k_nyquist_ratio[d] = k / k_nyq;
	}

	// precompute phase factors
	precompute_phases();

	// create solver and upload data
	m_solver = Solver::create(grid);
	upload_phases();
	m_solver->write_psi(psi_initial.data());

	// copy initial state into display buffers
	std::copy_n(psi_initial.data(), n, psi[0].data());
	std::copy_n(psi_initial.data(), n, psi[1].data());

	// apply absorbing boundary from setup
	if(setup.absorbing_boundary) {
		absorb_width = setup.absorb_width;
		absorb_strength = setup.absorb_strength;
		set_absorbing_boundary(true);
	}
}


void Simulation::compute_potential_phase()
{
	max_potential_phase = 0;
	grid.each([&](size_t idx, const int *coords, const double *pos) {
		double vr = potential[idx].real();
		double vi = potential[idx].imag();

		double phase = -vr * dt / (2.0 * hbar);
		double decay = -vi * dt / (2.0 * hbar);

		// absorbing boundary: always decays, independent of dt sign
		if(absorbing_boundary) {
			double absorb = 0;
			for(int d = 0; d < grid.rank; d++) {
				double L = grid.axes[d].max - grid.axes[d].min;
				double w = absorb_width * L;
				double dist_lo = pos[d] - grid.axes[d].min;
				double dist_hi = grid.axes[d].max - pos[d];
				if(dist_lo < w) {
					double t = cos(0.5 * M_PI * dist_lo / w);
					absorb += absorb_strength * t * t;
				}
				if(dist_hi < w) {
					double t = cos(0.5 * M_PI * dist_hi / w);
					absorb += absorb_strength * t * t;
				}
			}
			decay -= absorb * fabs(dt) / (2.0 * hbar);
		}

		double amp = exp(decay);
		m_potential_phase[idx] = psi_t(amp * cos(phase), amp * sin(phase));
		if(fabs(phase) > max_potential_phase) max_potential_phase = fabs(phase);
	});
}


void Simulation::compute_kinetic_phase()
{
	max_kinetic_phase = 0;
	grid.each([&](size_t idx, const int *coords, const double *pos) {
		double phase = 0;
		for(int d = 0; d < grid.rank; d++) {
			int ni = grid.axes[d].points;
			double L = grid.axes[d].max - grid.axes[d].min;
			double dk = 2.0 * M_PI / L;
			double ki = (coords[d] < ni/2) ? coords[d] * dk : (coords[d] - ni) * dk;
			phase += hbar * ki * ki * dt / (2.0 * mass[d]);
		}
		m_kinetic_phase[idx] = psi_t(cos(-phase), sin(-phase));
		if(fabs(phase) > max_kinetic_phase) max_kinetic_phase = fabs(phase);
	});
}


void Simulation::precompute_phases()
{
	compute_potential_phase();
	compute_kinetic_phase();
}


void Simulation::upload_phases()
{
	m_solver->set_phases(m_potential_phase.data(), m_kinetic_phase.data());
}


double Simulation::total_probability()
{
	return m_solver->total_probability(grid);
}

void Simulation::read_slice_1d(int axis, const int *cursor, psi_t *out)
{
	m_solver->read_slice_1d(grid, axis, cursor, out);
}

void Simulation::read_slice_2d(int ax_x, int ax_y, const int *cursor, psi_t *out)
{
	m_solver->read_slice_2d(grid, ax_x, ax_y, cursor, out);
}

void Simulation::read_marginal_1d(int axis, float *out, psi_t *coherent)
{
	m_solver->read_marginal_1d(grid, axis, out, coherent);
}

void Simulation::read_marginal_2d(int ax_x, int ax_y, float *out, psi_t *coherent)
{
	m_solver->read_marginal_2d(grid, ax_x, ax_y, out, coherent);
}


void Simulation::set_absorbing_boundary(bool on)
{
	if(on == absorbing_boundary) return;
	absorbing_boundary = on;

	// auto-compute strength if not already set
	if(on && absorb_strength <= 0) {
		double dx_min = grid.axes[0].dx();
		for(int d = 1; d < grid.rank; d++)
			if(grid.axes[d].dx() < dx_min) dx_min = grid.axes[d].dx();
		double k_max = M_PI / dx_min;
		double E_nyquist = hbar * hbar * k_max * k_max / (2.0 * mass[0]);
		double V_max = 0;
		size_t n = grid.total_points();
		for(size_t i = 0; i < n; i++) {
			double v = fabs(potential[i].real());
			if(v > V_max) V_max = v;
		}
		absorb_strength = fmax(V_max, E_nyquist * 0.01);
	}

	compute_potential_phase();
	upload_phases();
}


void Simulation::recompute_boundary()
{
	if(!absorbing_boundary) return;
	compute_potential_phase();
	upload_phases();
}


void Simulation::set_dt(double new_dt)
{
	if(new_dt == dt) return;
	dt = new_dt;
	precompute_phases();
	upload_phases();
}


void Simulation::reset()
{
	size_t n = grid.total_points();
	m_solver->write_psi(psi_initial.data());
	std::copy_n(psi_initial.data(), n, psi[0].data());
	std::copy_n(psi_initial.data(), n, psi[1].data());
	front.store(0);
	step_count = 0;
	sim_time = 0;
}


static std::mt19937 &rng()
{
	static std::mt19937 gen(std::random_device{}());
	return gen;
}


int Simulation::measure(int axis, double collapse_width)
{
	size_t n = grid.total_points();
	auto *p = psi_front();

	// default collapse width: 5% of domain along the measured axis
	if(collapse_width <= 0) {
		int ax = (axis >= 0 && axis < grid.rank) ? axis : 0;
		double L = grid.axes[ax].max - grid.axes[ax].min;
		collapse_width = L * 0.05;
	}

	// for multi-axis: if axis specified, measure along that axis only
	// by computing the marginal and sampling from it
	// for single axis (1D) or axis == -1: measure all axes at once

	if(axis >= 0 && axis < grid.rank && grid.rank > 1) {
		// marginal along this axis
		int na = grid.axes[axis].points;
		double dv = 1.0;
		for(int d = 0; d < grid.rank; d++)
			if(d != axis) dv *= grid.axes[d].dx();

		std::vector<double> prob(na, 0);
		grid.each([&](size_t idx, const int *coords, const double *pos) {
			prob[coords[axis]] += std::norm(p[idx]);
		});

		// build CDF
		std::vector<double> cdf(na);
		cdf[0] = prob[0];
		for(int i = 1; i < na; i++)
			cdf[i] = cdf[i-1] + prob[i];
		double total = cdf[na-1];
		if(total < 1e-30) return na / 2;

		// sample
		std::uniform_real_distribution<double> dist(0, total);
		double r = dist(rng());
		int result = 0;
		for(int i = 0; i < na; i++) {
			if(cdf[i] >= r) { result = i; break; }
		}

		// collapse: multiply psi by Gaussian along measured axis
		double x_measured = grid.axes[axis].min + result * grid.axes[axis].dx();
		double sigma2 = collapse_width * collapse_width;
		grid.each([&](size_t idx, const int *coords, const double *pos) {
			double dx = pos[axis] - x_measured;
			p[idx] *= (float)exp(-dx * dx / (2.0 * sigma2));
		});
		normalize_psi();
		commit_psi();
		return result;

	} else {
		// 1D or measure all axes: sample from full |psi|^2

		// build CDF
		std::vector<double> cdf(n);
		cdf[0] = std::norm(p[0]);
		for(size_t i = 1; i < n; i++)
			cdf[i] = cdf[i-1] + std::norm(p[i]);
		double total = cdf[n-1];
		if(total < 1e-30) return (int)(n / 2);

		// sample
		std::uniform_real_distribution<double> dist(0, total);
		double r = dist(rng());
		int result = 0;
		for(size_t i = 0; i < n; i++) {
			if(cdf[i] >= r) { result = (int)i; break; }
		}

		// collapse: Gaussian centered at measured position
		double sigma2 = collapse_width * collapse_width;
		int coords_result[MAX_RANK]{};
		grid.coords_from_index(result, coords_result);
		double pos_result[MAX_RANK];
		for(int d = 0; d < grid.rank; d++)
			pos_result[d] = grid.axes[d].min + coords_result[d] * grid.axes[d].dx();

		grid.each([&](size_t idx, const int *coords, const double *pos) {
			double r2 = 0;
			for(int d = 0; d < grid.rank; d++) {
				double dx = pos[d] - pos_result[d];
				r2 += dx * dx;
			}
			p[idx] *= (float)exp(-r2 / (2.0 * sigma2));
		});
		normalize_psi();
		commit_psi();
		return result;
	}
}


void Simulation::decohere(int axis, double strength)
{
	auto *p = psi_front();
	size_t n = grid.total_points();

	// linear phase ramp: adds a small momentum kick
	// each press accumulates — progressive kicks break coherence
	if(strength <= 0) strength = 0.1;

	grid.each([&](size_t idx, const int *coords, const double *pos) {
		double phi = 0;
		for(int d = 0; d < grid.rank; d++) {
			if(axis >= 0 && d != axis) continue;
			double L = grid.axes[d].max - grid.axes[d].min;
			phi += strength * M_PI * pos[d] / L;
		}
		p[idx] *= psi_t(cos(phi), sin(phi));
	});

	commit_psi();
}


// Renormalize the front psi buffer to total probability = 1
void Simulation::normalize_psi()
{
	auto *p = psi_front();
	size_t n = grid.total_points();
	double norm = 0;
	for(size_t i = 0; i < n; i++)
		norm += std::norm(p[i]);
	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	norm *= dv;
	if(norm > 1e-30) {
		float scale = (float)(1.0 / sqrt(norm));
		for(size_t i = 0; i < n; i++)
			p[i] *= scale;
	}
}


// Push modified CPU psi buffer to solver and swap display buffers
void Simulation::commit_psi()
{
	auto *p = psi_front();
	size_t n = grid.total_points();
	m_solver->write_psi(p);
	int back = 1 - front.load();
	std::copy_n(p, n, psi[back].data());
	front.store(back);
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
	m_psi_dirty = true;
}


void Simulation::flush()
{
	m_solver->flush();
}


void Simulation::sync()
{
	int back = 1 - front.load();
	m_solver->read_psi(psi[back].data());
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


static const double k_coulomb = 8.9875517873681764e9;  // N·m²/C²

void Simulation::sample_potential(const Setup &setup)
{
	grid.each([&](size_t idx, const int *coords, const double *pos) {
		psi_t v(0, 0);
		for(auto &pot : setup.potentials) {
			switch(pot.type) {
				case Potential::Type::Barrier:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v += pot.height;
					break;
				case Potential::Type::Well:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v -= pot.depth;
					break;
				case Potential::Type::Harmonic: {
					double r2 = 0;
					for(int d = 0; d < grid.rank; d++) {
						double dx = pos[d] - pot.center[d];
						r2 += dx * dx;
					}
					v += 0.5 * pot.k * r2;
					break;
				}
				case Potential::Type::Absorbing:
					if(inside_bounds(pos, pot.from, pot.to, grid.rank))
						v += psi_t(0, pot.height);
					break;
			}
		}

		// two-body interactions in configuration space
		for(auto &inter : setup.interactions) {
			int pa = inter.particle_a;
			int pb = inter.particle_b;
			if(pa >= cs.n_particles || pb >= cs.n_particles) continue;

			double dist2 = cs.distance_sq(pa, pb, pos);

			switch(inter.type) {
			case Interaction::Type::Coulomb: {
				double q_a = setup.particles[pa].charge;
				double q_b = setup.particles[pb].charge;
				double r = sqrt(dist2 + inter.softening * inter.softening);
				v += inter.strength * k_coulomb * q_a * q_b / r;
				break;
			}
				case Interaction::Type::Contact: {
					// Gaussian-shaped contact: smooth falloff, no hard wall
					double w2 = inter.width * inter.width;
					v += inter.strength * exp(-dist2 / w2);
					break;
				}
			}
		}

		potential[idx] = v;
	});
}


void Simulation::sample_wavefunction(const Setup &setup)
{
	if(setup.particles.empty()) return;

	double norm = 0;

	grid.each([&](size_t idx, const int *coords, const double *pos) {
		// product state: ψ(x₁,x₂,...) = ψ₁(x₁) · ψ₂(x₂) · ...
		// each particle contributes a Gaussian × plane wave across its axes
		psi_t val(1.0f, 0.0f);

		for(int p = 0; p < cs.n_particles; p++) {
			auto &part = setup.particles[p];
			double envelope = 0;
			double phase = 0;
			for(int d = 0; d < cs.spatial_dims; d++) {
				int ax = cs.axis(p, d);
				double dx = pos[ax] - part.position[d];
				envelope += dx * dx / (4.0 * part.width[d] * part.width[d]);
				phase += part.momentum[d] * pos[ax] / hbar;
			}
			double amp = exp(-envelope);
			val *= psi_t(amp * cos(phase), amp * sin(phase));
		}

		psi[0][idx] = val;
		norm += std::norm(val);
	});

	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	norm *= dv;
	double scale = 1.0 / sqrt(norm);

	size_t n = grid.total_points();
	float fscale = (float)scale;
	for(size_t i = 0; i < n; i++) {
		psi[0][i] *= fscale;
		// clamp float32 denormal noise to zero
		if(std::norm(psi[0][i]) < 1e-20f)
			psi[0][i] = psi_t(0, 0);
	}

	std::copy_n(psi[0].data(), n, psi_initial.data());
	std::copy_n(psi[0].data(), n, psi[1].data());
}


Simulation::~Simulation()
{
	// solver destroyed before vectors (implicit ordering is fine,
	// but explicit reset ensures GPU resources freed first)
	m_solver.reset();
}
