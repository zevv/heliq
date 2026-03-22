
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define POCKETFFT_NO_MULTITHREADING
#include "pocketfft_hdronly.h"

#include "solver_pocket.hpp"
#include "log.hpp"


PocketSolver::PocketSolver(const Grid &grid)
	: Solver(grid.total_points())
{
	m_rank = grid.rank;
	for(int i = 0; i < grid.rank; i++)
		m_dims[i] = grid.axes[i].points;

	size_t n = m_total;
	m_psi             = new psi_t[n];
	m_potential_phase  = new psi_t[n];
	m_kinetic_phase    = new psi_t[n];

	std::fill_n(m_psi,            n, psi_t(0));
	std::fill_n(m_potential_phase, n, psi_t(0));
	std::fill_n(m_kinetic_phase,   n, psi_t(0));

	linf("PocketFFT solver: rank=%d, total=%zu", m_rank, n);
}


PocketSolver::~PocketSolver()
{
	delete[] m_psi;
	delete[] m_potential_phase;
	delete[] m_kinetic_phase;
}


void PocketSolver::step()
{
	size_t n = m_total;

	// build pocketfft descriptors (on stack, cheap)
	pocketfft::shape_t shape(m_rank);
	pocketfft::stride_t stride(m_rank);
	pocketfft::shape_t axes(m_rank);

	// row-major strides in bytes
	size_t s = sizeof(psi_t);
	for(int d = m_rank - 1; d >= 0; d--) {
		shape[d] = (size_t)m_dims[d];
		stride[d] = (ptrdiff_t)s;
		axes[d] = (size_t)d;
		s *= m_dims[d];
	}

	float inv_n = 1.0f / (float)n;

	// 1. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];

	// 2. FFT forward
	pocketfft::c2c(shape, stride, stride, axes, pocketfft::FORWARD,
	               m_psi, m_psi, 1.0f);

	// 3. full-step kinetic
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_kinetic_phase[i];

	// 4. FFT inverse + normalize
	pocketfft::c2c(shape, stride, stride, axes, pocketfft::BACKWARD,
	               m_psi, m_psi, inv_n);

	// 5. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];
}


void PocketSolver::read_psi(psi_t *out) const
{
	std::copy_n(m_psi, m_total, out);
}


void PocketSolver::write_psi(const psi_t *in)
{
	std::copy_n(in, m_total, m_psi);
}


void PocketSolver::set_phases(const psi_t *potential_phase,
                              const psi_t *kinetic_phase)
{
	std::copy_n(potential_phase, m_total, m_potential_phase);
	std::copy_n(kinetic_phase,   m_total, m_kinetic_phase);
}
