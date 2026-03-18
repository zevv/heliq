#include "simcontext.hpp"
#include "experiment.hpp"
#include <cstdio>
#include <memory>

struct SimContext::Impl {
	Experiment exp{};
	int generation{};
};

SimContext::SimContext() : m_impl(std::make_unique<Impl>()) {}
SimContext::~SimContext() = default;


void SimContext::poll(double wall_dt)
{
	auto &exp = m_impl->exp;

	// drain all pending commands
	m_cmds.drain([&](SimCommand cmd) { handle(cmd); });

	// advance if running
	if(m_state.running && !exp.simulations.empty()) {
		exp.running = true;
		exp.advance(wall_dt);
	}

	// extract and publish
	if(!exp.simulations.empty()) {
		extract();
		publish();
	}

	// clear requests for next frame
	m_requests = {};
}


void SimContext::handle(SimCommand &cmd)
{
	auto &exp = m_impl->exp;

	std::visit([&](auto &c) {
		using T = std::decay_t<decltype(c)>;

		if constexpr (std::is_same_v<T, CmdLoad>) {
			if(exp.load(std::move(c.setup))) {
				m_impl->generation++;
			} else {
				m_state.error = "load failed";
			}
			m_state.running = false;
			m_state.generation = m_impl->generation;
		}
		else if constexpr (std::is_same_v<T, CmdAdvance>) {
			// handled in poll() via exp.advance()
		}
		else if constexpr (std::is_same_v<T, CmdSingleStep>) {
			for(auto &sim : exp.simulations)
				sim->step();
			if(!exp.simulations.empty())
				exp.sim_time = exp.simulations[0]->time();
		}
		else if constexpr (std::is_same_v<T, CmdSetDt>) {
			for(auto &sim : exp.simulations)
				sim->set_dt(c.dt);
		}
		else if constexpr (std::is_same_v<T, CmdSetTimescale>) {
			exp.timescale = c.ts;
		}
		else if constexpr (std::is_same_v<T, CmdSetRunning>) {
			m_state.running = c.run;
			exp.running = c.run;
		}
		else if constexpr (std::is_same_v<T, CmdMeasure>) {
			for(auto &sim : exp.simulations)
				sim->measure(c.axis);
		}
		else if constexpr (std::is_same_v<T, CmdDecohere>) {
			for(auto &sim : exp.simulations)
				sim->decohere(c.axis);
		}
		else if constexpr (std::is_same_v<T, CmdSetAbsorb>) {
			for(auto &sim : exp.simulations) {
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
	auto &exp = m_impl->exp;
	auto &sim = *exp.simulations[0];
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
	auto &exp = m_impl->exp;
	auto &sim = *exp.simulations[0];

	m_state.sim_time = exp.sim_time;
	m_state.step_count = sim.step_count;
	m_state.total_probability = sim.total_probability();
	m_state.phase_v = sim.max_potential_phase;
	m_state.phase_k = sim.max_kinetic_phase;
	m_state.dt = sim.dt;
	m_state.timescale = exp.timescale;
	m_state.generation = m_impl->generation;
	m_state.error.clear();

	for(int d = 0; d < sim.grid.rank; d++)
		m_state.k_nyquist_ratio[d] = sim.k_nyquist_ratio[d];

	m_state.grid.rank = sim.grid.rank;
	for(int d = 0; d < sim.grid.rank; d++)
		m_state.grid.axes[d] = sim.grid.axes[d];
	m_state.grid.cs = sim.cs;

	m_state.setup = exp.setup;

	m_state.absorbing_boundary = sim.absorbing_boundary;
	m_state.absorb_width = sim.absorb_width;
	m_state.absorb_strength = sim.absorb_strength;

	// TODO: marginal peaks from extraction results
}
