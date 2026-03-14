#pragma once

#include "solver.hpp"

class CpuSolver : public Solver {
public:
	CpuSolver(const Grid &grid);
	~CpuSolver() override;

	void step() override;
	void read_psi(std::complex<double> *out) const override;
	void write_psi(const std::complex<double> *in) override;
	void set_phases(const std::complex<double> *potential_phase,
	                const std::complex<double> *kinetic_phase) override;

private:
	int m_rank{};
	int m_dims[MAX_RANK]{};

	std::complex<double> *m_psi{};
	std::complex<double> *m_potential_phase{};
	std::complex<double> *m_kinetic_phase{};

	void *m_fft_forward{};
	void *m_fft_inverse{};
};
