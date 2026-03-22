#pragma once

#include "solver.hpp"

class PocketSolver : public Solver {
public:
	PocketSolver(const Grid &grid);
	~PocketSolver() override;

	void step() override;
	void read_psi(psi_t *out) const override;
	void write_psi(const psi_t *in) override;
	void set_phases(const psi_t *potential_phase,
	                const psi_t *kinetic_phase) override;

private:
	int m_rank{};
	int m_dims[MAX_RANK]{};

	psi_t *m_psi{};
	psi_t *m_potential_phase{};
	psi_t *m_kinetic_phase{};
};
