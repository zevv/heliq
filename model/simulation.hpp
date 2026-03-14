#pragma once

#include <complex>
#include <atomic>

#include "grid.hpp"
#include "fft.hpp"

class Simulation {
public:
	Grid grid{};
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

	// FFT plan (opaque, owned by backend)
	Fft::Plan *fft_plan{};

	std::complex<double> *psi_front() { return psi[front.load()]; }
};
