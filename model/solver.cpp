
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "solver.hpp"
#include "solver_cpu.hpp"
#include "solver_gpu.hpp"

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

	// default: prefer GPU
	if(GpuSolver::available())
		return std::make_unique<GpuSolver>(grid);

	fprintf(stderr, "solver: using CPU (FFTW)\n");
	return std::make_unique<CpuSolver>(grid);
}
