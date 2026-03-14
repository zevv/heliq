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
	void read_psi(std::complex<double> *out) const override;
	void write_psi(const std::complex<double> *in) override;
	void set_phases(const std::complex<double> *potential_phase,
	                const std::complex<double> *kinetic_phase) override;

	// returns true if an OpenCL GPU device is available
	static bool available();

private:
	GpuSolverImpl *m{};
};
