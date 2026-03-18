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

	double total_probability(const Grid &grid) override;
	void read_slice_1d(const Grid &grid, int axis,
	                   const int *cursor, psi_t *out) override;
	void read_slice_2d(const Grid &grid, int ax_x, int ax_y,
	                   const int *cursor, psi_t *out) override;
	void read_marginal_1d(const Grid &grid, int axis, float *out) override;
	void read_marginal_2d(const Grid &grid, int ax_x, int ax_y, float *out) override;

	// returns true if an OpenCL GPU device is available
	static bool available();

private:
	GpuSolverImpl *m{};
};
