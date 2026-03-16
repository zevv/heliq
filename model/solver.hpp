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
	virtual void read_psi(psi_t *out) const = 0;

	// upload psi from CPU buffer (for reset / init)
	virtual void write_psi(const psi_t *in) = 0;

	// upload new phase arrays (after dt change)
	virtual void set_phases(const psi_t *potential_phase,
	                        const psi_t *kinetic_phase) = 0;

	// grid point count
	size_t total_points() const { return m_total; }

	// factory: probe backends, create best available solver
	static std::unique_ptr<Solver> create(const Grid &grid);

protected:
	Solver(size_t total) : m_total(total) {}
	size_t m_total{};
};
