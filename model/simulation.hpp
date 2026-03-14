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

	void step();
	void set_dt(double new_dt);

	Grid grid{};
	std::string name{};
	SimMode mode{SimMode::Joint};
	double dt{};
	double mass{};          // kg, first particle for now
	size_t step_count{};

	double time() const { return step_count * dt; }

	// CPU-side wavefunction double buffer (for widget reads)
	std::complex<double> *psi[2]{};
	std::atomic<int> front{0};

	// potential (CPU-side, for widget display)
	std::complex<double> *potential{};

	// initial state snapshot for reset
	std::complex<double> *psi_initial{};

	// read access for widgets
	std::complex<double> *psi_front() { return psi[front.load()]; }

	// diagnostics
	double max_potential_phase{};  // max |V*dt/(2*hbar)| in radians
	double max_kinetic_phase{};    // max |hbar*k^2/(2m)*dt| in radians
	double total_probability();    // should be ~1.0

private:
	void sample_potential(const Setup &setup);
	void sample_wavefunction(const Setup &setup);
	void precompute_phases();
	void upload_phases();

	std::unique_ptr<Solver> m_solver{};

	// CPU-side phase arrays (computed here, uploaded to solver)
	std::complex<double> *m_potential_phase{};
	std::complex<double> *m_kinetic_phase{};
};
