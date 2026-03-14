
#include <stdio.h>

#include "solver.hpp"
#include "solver_cpu.hpp"

std::unique_ptr<Solver> Solver::create(const Grid &grid)
{
	// TODO: probe GPU backends (OpenCL, etc.) and prefer those
	fprintf(stderr, "solver: using CPU (FFTW)\n");
	return std::make_unique<CpuSolver>(grid);
}
