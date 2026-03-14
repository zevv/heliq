#pragma once

#include <complex>
#include <memory>

#include "grid.hpp"

// Abstract split-step Fourier solver.
// Owns all hot-path data (psi, phase arrays, FFT plans).
// CPU and GPU implementations behind this interface.

class Solver {
public:
	virtual ~Solver() = default;

	// no copy
	Solver(const Solver &) = delete;
	Solver &operator=(const Solver &) = delete;

	// one full split-step iteration
	virtual void step() = 0;

	// wait for all queued work to complete (GPU: clFinish, CPU: no-op)
	virtual void flush() {}

	// copy current psi to CPU buffer (for visualization)
	virtual void read_psi(std::complex<double> *out) const = 0;

	// upload psi from CPU buffer (for reset / init)
	virtual void write_psi(const std::complex<double> *in) = 0;

	// upload new phase arrays (after dt change)
	virtual void set_phases(const std::complex<double> *potential_phase,
	                        const std::complex<double> *kinetic_phase) = 0;

	// grid point count
	size_t total_points() const { return m_total; }

	// factory: probe backends, create best available solver
	static std::unique_ptr<Solver> create(const Grid &grid);

protected:
	Solver(size_t total) : m_total(total) {}
	size_t m_total{};
};
