#pragma once

#include <complex>
#include <atomic>
#include <string>

#include "grid.hpp"
#include "setup.hpp"

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

	// wavefunction double buffer
	std::complex<double> *psi[2]{};
	std::atomic<int> front{0};

	// potential (same grid shape)
	std::complex<double> *potential{};

	// precomputed phase factors
	std::complex<double> *potential_phase{};  // exp(-i V dt / 2hbar)
	std::complex<double> *kinetic_phase{};    // exp(-i hbar k^2 dt / 2m)

	// initial state snapshot for reset
	std::complex<double> *psi_initial{};

	// read access for widgets
	std::complex<double> *psi_front() { return psi[front.load()]; }

private:
	void sample_potential(const Setup &setup);
	void sample_wavefunction(const Setup &setup);
	void precompute_phases();

	// FFTW plans
	void *fft_forward{};
	void *fft_inverse{};
};
