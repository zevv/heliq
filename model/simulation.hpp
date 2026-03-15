#pragma once

#include <complex>
#include <atomic>
#include <string>
#include <memory>

#include "grid.hpp"
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

	Grid grid{};
	std::string name{};
	SimMode mode{SimMode::Joint};
	double dt{};
	double mass[MAX_RANK]{};  // kg, per axis (particle mass for that config-space axis)
	size_t step_count{};
	double sim_time{};      // accumulated simulation time (supports negative dt)

	double time() const { return sim_time; }

	// CPU-side wavefunction double buffer (for widget reads)
	std::complex<double> *psi[2]{};
	std::atomic<int> front{0};

	// potential (CPU-side, for widget display)
	std::complex<double> *potential{};

	// initial state snapshot for reset
	std::complex<double> *psi_initial{};

	// read access for widgets
	std::complex<double> *psi_front() { return psi[front.load()]; }

	// absorbing boundary
	bool absorbing_boundary{false};
	double absorb_width{0.02};    // fraction of domain width per side
	double absorb_strength{};     // auto-computed from dt
	void set_absorbing_boundary(bool on);
	void recompute_boundary();  // re-apply after width/strength change

	// diagnostics
	double max_potential_phase{};  // max |V*dt/(2*hbar)| in radians
	double max_kinetic_phase{};    // max |hbar*k^2/(2m)*dt| in radians
	double total_probability();    // should be ~1.0

private:
	void sample_potential(const Setup &setup);
	void sample_wavefunction(const Setup &setup);
	void precompute_phases();
	void compute_potential_phase();
	void compute_kinetic_phase();
	void upload_phases();

	std::unique_ptr<Solver> m_solver{};

	// CPU-side phase arrays (computed here, uploaded to solver)
	std::complex<double> *m_potential_phase{};
	std::complex<double> *m_kinetic_phase{};
};
