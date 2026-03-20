
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

#include "solver.hpp"
#include "solver_cpu.hpp"
#include "solver_gpu.hpp"


// --- default (CPU fallback) implementations of extraction methods ---

double Solver::total_probability(const Grid &grid)
{
	std::vector<psi_t> tmp(m_total);
	flush();
	read_psi(tmp.data());
	double sum = 0;
	for(size_t i = 0; i < m_total; i++)
		sum += std::norm(tmp[i]);
	double dv = 1.0;
	for(int d = 0; d < grid.rank; d++)
		dv *= grid.axes[d].dx();
	return sum * dv;
}

void Solver::read_slice_1d(const Grid &grid, int axis,
                            const int *cursor, psi_t *out)
{
	std::vector<psi_t> tmp(m_total);
	flush();
	read_psi(tmp.data());
	auto view = grid.axis_view(axis, cursor, tmp.data());
	for(auto val : view)
		*out++ = val;
}

void Solver::read_slice_2d(const Grid &grid, int ax_x, int ax_y,
                            const int *cursor, psi_t *out)
{
	std::vector<psi_t> tmp(m_total);
	flush();
	read_psi(tmp.data());
	auto view = grid.slice_view(ax_x, ax_y, cursor, tmp.data());
	int nx = view.nx, ny = view.ny;
	for(int x = 0; x < nx; x++)
		for(int y = 0; y < ny; y++)
			out[x * ny + y] = view.at(x, y);
}

void Solver::read_marginal_1d(const Grid &grid, int axis, float *out, psi_t *coherent)
{
	std::vector<psi_t> tmp(m_total);
	flush();
	read_psi(tmp.data());
	int n = grid.axes[axis].points;
	std::fill_n(out, n, 0.0f);
	if(coherent) std::fill_n(coherent, n, psi_t(0, 0));

	int coords[MAX_RANK]{};
	for(size_t idx = 0; idx < m_total; idx++) {
		int i = coords[axis];
		out[i] += std::norm(tmp[idx]);
		if(coherent) coherent[i] += tmp[idx];
		for(int d = grid.rank - 1; d >= 0; d--) {
			if(++coords[d] < grid.axes[d].points) break;
			coords[d] = 0;
		}
	}
}

void Solver::read_marginal_2d(const Grid &grid, int ax_x, int ax_y, float *out, psi_t *coherent)
{
	std::vector<psi_t> tmp(m_total);
	flush();
	read_psi(tmp.data());
	int nx = grid.axes[ax_x].points;
	int ny = grid.axes[ax_y].points;
	int ntot = nx * ny;
	std::fill_n(out, ntot, 0.0f);
	if(coherent) std::fill_n(coherent, ntot, psi_t(0, 0));

	int coords[MAX_RANK]{};
	for(size_t idx = 0; idx < m_total; idx++) {
		int ix = coords[ax_x];
		int iy = coords[ax_y];
		int oi = ix * ny + iy;
		out[oi] += std::norm(tmp[idx]);
		if(coherent) coherent[oi] += tmp[idx];
		for(int d = grid.rank - 1; d >= 0; d--) {
			if(++coords[d] < grid.axes[d].points) break;
			coords[d] = 0;
		}
	}
}

std::unique_ptr<Solver> Solver::create(const Grid &grid)
{
	const char *env = getenv("QUANTUM_SOLVER");

	if(env && strcmp(env, "cpu") == 0) {
		fprintf(stderr, "solver: using CPU (FFTW) [QUANTUM_SOLVER=cpu]\n");
		return std::make_unique<CpuSolver>(grid);
	}

	if(env && strcmp(env, "gpu") == 0) {
		if(!GpuSolver::available()) {
			fprintf(stderr, "solver: GPU requested but not available, falling back to CPU\n");
			return std::make_unique<CpuSolver>(grid);
		}
		return std::make_unique<GpuSolver>(grid);
	}

	// default: prefer GPU (rank > 3 uses transpose decomposition)
	if(GpuSolver::available())
		return std::make_unique<GpuSolver>(grid);

	fprintf(stderr, "solver: using CPU (FFTW)\n");
	return std::make_unique<CpuSolver>(grid);
}
