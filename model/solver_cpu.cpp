
#include <fftw3.h>
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <sys/stat.h>

#include "solver_cpu.hpp"
#include "log.hpp"

static bool fftwf_threads_init = false;

static void wisdom_path(char *buf, size_t len)
{
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");
	if(cache)
		snprintf(buf, len, "%s/heliq", cache);
	else if(home)
		snprintf(buf, len, "%s/.cache/heliq", home);
	else
		snprintf(buf, len, "./.heliq-cache");
	mkdir(buf, 0755);
	size_t n = strlen(buf);
	snprintf(buf + n, len - n, "/fftwf_wisdom");
}

CpuSolver::CpuSolver(const Grid &grid)
	: Solver(grid.total_points())
{
	if(!fftwf_threads_init) {
		fftwf_init_threads();
		fftwf_plan_with_nthreads(std::thread::hardware_concurrency());
		fftwf_threads_init = true;
	}

	// load wisdom
	char wpath[512];
	wisdom_path(wpath, sizeof(wpath));
	if(fftwf_import_wisdom_from_filename(wpath))
		ldbg("loaded wisdom from %s", wpath);

	m_rank = grid.rank;
	for(int i = 0; i < grid.rank; i++)
		m_dims[i] = grid.axes[i].points;

	size_t n = m_total;
	size_t bytes = n * sizeof(psi_t);

	m_psi             = (psi_t *)fftwf_malloc(bytes);
	m_potential_phase  = (psi_t *)fftwf_malloc(bytes);
	m_kinetic_phase    = (psi_t *)fftwf_malloc(bytes);

	std::fill_n(m_psi,             n, psi_t(0));
	std::fill_n(m_potential_phase,  n, psi_t(0));
	std::fill_n(m_kinetic_phase,    n, psi_t(0));

	// create FFTW plans (MEASURE may trash m_psi, caller must write_psi after)
	m_fft_forward = fftwf_plan_dft(m_rank, m_dims,
		(fftwf_complex *)m_psi, (fftwf_complex *)m_psi,
		FFTW_FORWARD, FFTW_MEASURE);

	m_fft_inverse = fftwf_plan_dft(m_rank, m_dims,
		(fftwf_complex *)m_psi, (fftwf_complex *)m_psi,
		FFTW_BACKWARD, FFTW_MEASURE);

	// save wisdom (includes new plans)
	if(fftwf_export_wisdom_to_filename(wpath))
		ldbg("saved wisdom to %s", wpath);
}


CpuSolver::~CpuSolver()
{
	if(m_fft_forward) fftwf_destroy_plan(m_fft_forward);
	if(m_fft_inverse) fftwf_destroy_plan(m_fft_inverse);
	fftwf_free(m_psi);
	fftwf_free(m_potential_phase);
	fftwf_free(m_kinetic_phase);
}


void CpuSolver::step()
{
	size_t n = m_total;

	// 1. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];

	// 2. FFT forward
	fftwf_execute_dft(m_fft_forward, (fftwf_complex *)m_psi, (fftwf_complex *)m_psi);

	// 3. full-step kinetic
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_kinetic_phase[i];

	// 4. FFT inverse
	fftwf_execute_dft(m_fft_inverse, (fftwf_complex *)m_psi, (fftwf_complex *)m_psi);

	// FFTW inverse doesn't normalize
	float inv_n = 1.0f / (float)n;
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= inv_n;

	// 5. half-step potential
	for(size_t i = 0; i < n; i++)
		m_psi[i] *= m_potential_phase[i];
}


void CpuSolver::read_psi(psi_t *out) const
{
	std::copy_n(m_psi, m_total, out);
}


void CpuSolver::write_psi(const psi_t *in)
{
	std::copy_n(in, m_total, m_psi);
}


void CpuSolver::set_phases(const psi_t *potential_phase,
                            const psi_t *kinetic_phase)
{
	std::copy_n(potential_phase, m_total, m_potential_phase);
	std::copy_n(kinetic_phase,   m_total, m_kinetic_phase);
}
