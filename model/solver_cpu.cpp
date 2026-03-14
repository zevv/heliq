
#include <fftw3.h>
#include <algorithm>
#include <math.h>
#include <thread>

#include "solver_cpu.hpp"

static bool fftw_threads_initialized = false;

CpuSolver::CpuSolver(const Grid &grid)
	: Solver(grid.total_points())
{
	if(!fftw_threads_initialized) {
		fftw_init_threads();
		fftw_plan_with_nthreads(std::thread::hardware_concurrency());
		fftw_threads_initialized = true;
	}

	m_rank = grid.rank;
	for(int i = 0; i < grid.rank; i++)
		m_dims[i] = grid.axes[i].points;

	size_t n = m_total;
	size_t bytes = n * sizeof(std::complex<double>);

	m_psi             = (std::complex<double> *)fftw_malloc(bytes);
	m_potential_phase  = (std::complex<double> *)fftw_malloc(bytes);
	m_kinetic_phase    = (std::complex<double> *)fftw_malloc(bytes);

	std::fill_n(m_psi,             n, std::complex<double>(0));
	std::fill_n(m_potential_phase,  n, std::complex<double>(0));
	std::fill_n(m_kinetic_phase,    n, std::complex<double>(0));

	// create FFTW plans (MEASURE may trash m_psi, caller must write_psi after)
	m_fft_forward = fftw_plan_dft(m_rank, m_dims,
		(fftw_complex *)m_psi, (fftw_complex *)m_psi,
		FFTW_FORWARD, FFTW_MEASURE);

	m_fft_inverse = fftw_plan_dft(m_rank, m_dims,
		(fftw_complex *)m_psi, (fftw_complex *)m_psi,
		FFTW_BACKWARD, FFTW_MEASURE);
}


CpuSolver::~CpuSolver()
{
	if(m_fft_forward) fftw_destroy_plan((fftw_plan)m_fft_forward);
	if(m_fft_inverse) fftw_destroy_plan((fftw_plan)m_fft_inverse);
	fftw_free(m_psi);
	fftw_free(m_potential_phase);
	fftw_free(m_kinetic_phase);
}


void CpuSolver::step()
{
	size_t n = m_total;

	// 1. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];

	// 2. FFT forward
	fftw_execute_dft((fftw_plan)m_fft_forward, (fftw_complex *)m_psi, (fftw_complex *)m_psi);

	// 3. full-step kinetic
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_kinetic_phase[i];

	// 4. FFT inverse
	fftw_execute_dft((fftw_plan)m_fft_inverse, (fftw_complex *)m_psi, (fftw_complex *)m_psi);

	// FFTW inverse doesn't normalize
	double inv_n = 1.0 / (double)n;
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= inv_n;

	// 5. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];
}


void CpuSolver::read_psi(std::complex<double> *out) const
{
	std::copy_n(m_psi, m_total, out);
}


void CpuSolver::write_psi(const std::complex<double> *in)
{
	std::copy_n(in, m_total, m_psi);
}


void CpuSolver::set_phases(const std::complex<double> *potential_phase,
                            const std::complex<double> *kinetic_phase)
{
	std::copy_n(potential_phase, m_total, m_potential_phase);
	std::copy_n(kinetic_phase,   m_total, m_kinetic_phase);
}
