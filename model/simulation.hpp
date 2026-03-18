#pragma once

#include <complex>
#include <atomic>
#include <string>
#include <memory>
#include <vector>

#include "grid.hpp"
#include "configspace.hpp"
#include "setup.hpp"
#include "solver.hpp"

class Simulation {
public:
	Simulation(const SimConfig &config, const Setup &setup);
	~Simulation();

	// no copy
	Simulation(const Simulation &) = delete;
	Simulation &operator=(const Simulation &) = delete;

	void step();          // compute + flush + readback
	void step_compute();  // enqueue compute (may return before GPU finishes)
	void flush();         // wait for enqueued compute to finish
	void sync();          // flush + readback to CPU display buffer
	void set_dt(double new_dt);
	void reset();         // restore initial state, reset time
	int measure(int axis = -1, double collapse_width = 0);  // measure position, returns grid index
	void decohere(int axis = -1, double width = 0);          // randomize phase, keep amplitude

	Grid grid{};
	ConfigSpace cs{};
	std::string name{};
	SimMode mode{SimMode::Joint};
	double dt{};
	double mass[MAX_RANK]{};  // kg, per axis (particle mass for that config-space axis)
	size_t step_count{};
	double sim_time{};      // accumulated simulation time (supports negative dt)

	double time() const { return sim_time; }

	// CPU-side wavefunction double buffer (for widget reads)
	std::vector<psi_t> psi[2];
	std::atomic<int> front{0};

	// potential (CPU-side, for widget display)
	std::vector<psi_t> potential;

	// initial state snapshot for reset
	std::vector<psi_t> psi_initial;

	// read access for widgets (triggers GPU readback if stale)
	psi_t *psi_front() {
		if(m_psi_dirty) { sync(); m_psi_dirty = false; }
		return psi[front.load()].data();
	}
	void mark_dirty() { m_psi_dirty = true; }
	void normalize_psi();  // renormalize front psi to probability 1
	void commit_psi();     // push modified CPU psi to solver, swap display buffers

	// absorbing boundary
	bool absorbing_boundary{false};
	double absorb_width{0.02};    // fraction of domain width per side
	double absorb_strength{};     // auto-computed from dt
	void set_absorbing_boundary(bool on);
	void recompute_boundary();  // re-apply after width/strength change

	// diagnostics
	double max_potential_phase{};  // max |V*dt/(2*hbar)| in radians
	double max_kinetic_phase{};    // max |hbar*k^2/(2m)*dt| in radians
	double k_nyquist_ratio[MAX_RANK]{};  // per-axis: initial k / k_nyquist (>0.5 = trouble)
	double total_probability();    // should be ~1.0

	// GPU-side data extraction (avoids full readback)
	void read_slice_1d(int axis, const int *cursor, psi_t *out);
	void read_slice_2d(int ax_x, int ax_y, const int *cursor, psi_t *out);
	void read_marginal_1d(int axis, float *out);
	void read_marginal_2d(int ax_x, int ax_y, float *out);

private:
	void sample_potential(const Setup &setup);
	void sample_wavefunction(const Setup &setup);
	void precompute_phases();
	void compute_potential_phase();
	void compute_kinetic_phase();
	void upload_phases();

	bool m_psi_dirty{false};
	std::unique_ptr<Solver> m_solver{};

	// CPU-side phase arrays (computed here, uploaded to solver)
	std::vector<psi_t> m_potential_phase;
	std::vector<psi_t> m_kinetic_phase;
};
