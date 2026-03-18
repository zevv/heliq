#include "simcontext.hpp"
#include <cstdio>


void SimContext::poll(double wall_dt)
{
	// drain all pending commands
	m_cmds.drain([&](SimCommand cmd) { handle(cmd); });

	// advance if running
	if(m_state.running && !m_exp.simulations.empty()) {
		m_exp.running = true;
		m_exp.advance(wall_dt);
	}

	// extract and publish
	if(!m_exp.simulations.empty()) {
		extract();
		publish();
	}

	// clear requests for next frame
	m_requests = {};
}


void SimContext::handle(SimCommand &cmd)
{
	std::visit([&](auto &c) {
		using T = std::decay_t<decltype(c)>;

		if constexpr (std::is_same_v<T, CmdLoad>) {
			if(m_exp.load(std::move(c.setup))) {
				m_generation++;
			} else {
				m_state.error = "load failed";
			}
			m_state.running = false;
			m_state.generation = m_generation;
		}
		else if constexpr (std::is_same_v<T, CmdAdvance>) {
			// handled in poll() via m_exp.advance()
		}
		else if constexpr (std::is_same_v<T, CmdSingleStep>) {
			for(auto &sim : m_exp.simulations)
				sim->step();
			if(!m_exp.simulations.empty())
				m_exp.sim_time = m_exp.simulations[0]->time();
		}
		else if constexpr (std::is_same_v<T, CmdSetDt>) {
			for(auto &sim : m_exp.simulations)
				sim->set_dt(c.dt);
		}
		else if constexpr (std::is_same_v<T, CmdSetTimescale>) {
			m_exp.timescale = c.ts;
		}
		else if constexpr (std::is_same_v<T, CmdSetRunning>) {
			m_state.running = c.run;
			m_exp.running = c.run;
		}
		else if constexpr (std::is_same_v<T, CmdMeasure>) {
			for(auto &sim : m_exp.simulations)
				sim->measure(c.axis);
		}
		else if constexpr (std::is_same_v<T, CmdDecohere>) {
			for(auto &sim : m_exp.simulations)
				sim->decohere(c.axis);
		}
		else if constexpr (std::is_same_v<T, CmdSetAbsorb>) {
			for(auto &sim : m_exp.simulations) {
				sim->set_absorbing_boundary(c.on);
				sim->absorb_width = c.width;
				sim->absorb_strength = c.strength;
				sim->recompute_boundary();
			}
		}
	}, cmd);
}


void SimContext::extract()
{
	auto &sim = *m_exp.simulations[0];
	m_state.n_results = 0;

	for(int i = 0; i < m_requests.count; i++) {
		auto &req = m_requests.req[i];
		auto &res = m_state.results[m_state.n_results];

		// count free axes
		int n_axes = 0;
		for(int a = 0; a < MAX_RANK && req.axes[a] >= 0; a++)
			n_axes++;
		if(n_axes == 0) continue;

		// copy request info
		for(int a = 0; a < MAX_RANK; a++) res.axes[a] = req.axes[a];
		res.marginal = req.marginal;

		// shape
		for(int a = 0; a < n_axes; a++)
			res.shape[a] = sim.grid.axes[req.axes[a]].points;
		for(int a = n_axes; a < MAX_RANK; a++)
			res.shape[a] = -1;

		int total = 1;
		for(int a = 0; a < n_axes; a++)
			total *= res.shape[a];

		res.psi.resize(total);
		res.pot.resize(total);
		res.coherent.clear();

		if(req.marginal) {
			// |ψ|² marginal
			std::vector<float> marg(total);
			if(n_axes == 1)
				sim.read_marginal_1d(req.axes[0], marg.data());
			else if(n_axes == 2)
				sim.read_marginal_2d(req.axes[0], req.axes[1], marg.data());
			for(int j = 0; j < total; j++)
				res.psi[j] = psi_t(marg[j], 0);

			// TODO: |ψ|²-weighted potential marginal
			std::fill(res.pot.begin(), res.pot.end(), psi_t(0, 0));

			// TODO: coherent marginal ∫ψ
			res.coherent.resize(total, psi_t(0, 0));
		} else {
			// psi slice
			if(n_axes == 1)
				sim.read_slice_1d(req.axes[0], req.cursor, res.psi.data());
			else if(n_axes == 2)
				sim.read_slice_2d(req.axes[0], req.axes[1], req.cursor,
					res.psi.data());

			// potential slice (CPU-side, cheap)
			if(n_axes == 1) {
				auto pot_view = sim.grid.axis_view(req.axes[0], req.cursor, sim.potential.data());
				int idx = 0;
				for(auto val : pot_view) {
					if(idx >= total) break;
					res.pot[idx++] = val;
				}
			} else if(n_axes == 2) {
				auto pot_view = sim.grid.slice_view(req.axes[0], req.axes[1], req.cursor, sim.potential.data());
				for(int x = 0; x < res.shape[0]; x++)
					for(int y = 0; y < res.shape[1]; y++)
						res.pot[x * res.shape[1] + y] = pot_view.at(x, y);
			}
		}

		m_state.n_results++;
	}
}


void SimContext::publish()
{
	auto &sim = *m_exp.simulations[0];

	m_state.sim_time = m_exp.sim_time;
	m_state.step_count = sim.step_count;
	m_state.total_probability = sim.total_probability();
	m_state.phase_v = sim.max_potential_phase;
	m_state.phase_k = sim.max_kinetic_phase;
	m_state.dt = sim.dt;
	m_state.timescale = m_exp.timescale;
	m_state.generation = m_generation;
	m_state.error.clear();

	for(int d = 0; d < sim.grid.rank; d++)
		m_state.k_nyquist_ratio[d] = sim.k_nyquist_ratio[d];

	m_state.grid.rank = sim.grid.rank;
	for(int d = 0; d < sim.grid.rank; d++)
		m_state.grid.axes[d] = sim.grid.axes[d];
	m_state.grid.cs = sim.cs;

	m_state.setup = m_exp.setup;

	m_state.absorbing_boundary = sim.absorbing_boundary;
	m_state.absorb_width = sim.absorb_width;
	m_state.absorb_strength = sim.absorb_strength;

	// TODO: marginal peaks from extraction results
}
