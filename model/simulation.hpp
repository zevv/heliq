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

	Grid grid{};
	std::string name{};
	SimMode mode{SimMode::Joint};
	double dt{};
	size_t step_count{};

	double time() const { return step_count * dt; }

	// wavefunction double buffer
	std::complex<double> *psi[2]{};
	std::atomic<int> front{0};

	// potential (same grid shape)
	std::complex<double> *potential{};

	// initial state snapshot for reset
	std::complex<double> *psi_initial{};

	// read access for widgets
	std::complex<double> *psi_front() { return psi[front.load()]; }
};
