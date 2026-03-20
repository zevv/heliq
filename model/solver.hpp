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

	// --- GPU-side extraction (avoids full readback) ---

	// total probability (sum |psi|^2 * dx^N), default: read_psi + CPU sum
	virtual double total_probability(const Grid &grid);

	// extract 1D slice at cursor into out[n], default: read_psi + CPU extract
	virtual void read_slice_1d(const Grid &grid, int axis,
	                           const int *cursor, psi_t *out);

	// extract 2D slice at cursor into out[nx*ny], default: read_psi + CPU extract
	virtual void read_slice_2d(const Grid &grid, int ax_x, int ax_y,
	                           const int *cursor, psi_t *out);

	// compute 1D marginal (sum over all other axes) into out[n], default: read_psi + CPU sum
	virtual void read_marginal_1d(const Grid &grid, int axis, float *out, psi_t *coherent = nullptr);

	// compute 2D marginal (sum over hidden axes) into out[nx*ny], default: read_psi + CPU sum
	virtual void read_marginal_2d(const Grid &grid, int ax_x, int ax_y, float *out, psi_t *coherent = nullptr);

	// grid point count
	size_t total_points() const { return m_total; }

	// factory: probe backends, create best available solver
	static std::unique_ptr<Solver> create(const Grid &grid);

protected:
	Solver(size_t total) : m_total(total) {}
	size_t m_total{};
};
