#pragma once

#include "solver.hpp"

// Forward declarations — OpenCL and VkFFT types are opaque here
struct GpuSolverImpl;

class GpuSolver : public Solver {
public:
	GpuSolver(const Grid &grid);
	~GpuSolver() override;

	void step() override;
	void flush() override;
	void read_psi(psi_t *out) const override;
	void write_psi(const psi_t *in) override;
	void set_phases(const psi_t *potential_phase,
	                const psi_t *kinetic_phase) override;

	// returns true if an OpenCL GPU device is available
	static bool available();

private:
	GpuSolverImpl *m{};
};
