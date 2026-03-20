#define LOG_TAG "simthread"

#include "simthread.hpp"
#include "log.hpp"


SimThread::SimThread() {}

SimThread::~SimThread() { stop(); }


void SimThread::start()
{
	if(m_alive.load()) return;
	m_alive.store(true);
	m_thread = std::thread(&SimThread::run, this);
}


void SimThread::stop()
{
	if(!m_alive.load()) return;
	m_alive.store(false);
	m_cmds.wake();
	if(m_thread.joinable())
		m_thread.join();
}


void SimThread::publish_requests(const ExtractionSet &set)
{
	m_requests.write_buf() = set;
	m_requests.publish();
}


// --- sim thread ---

void SimThread::run()
{
	linf("started");

	while(m_alive.load()) {
		// drain commands
		m_cmds.drain([&](SimCommand cmd) { handle(std::move(cmd)); });

		if(!m_running || m_exp.simulations.empty()) {
			// idle: wait for commands
			m_cmds.wait_for(std::chrono::milliseconds(50));
			continue;
		}
	}

	linf("stopped");
}


// --- command handlers ---

void SimThread::handle(SimCommand cmd)
{
	std::visit([&](auto &c) {
		using T = std::decay_t<decltype(c)>;

		if constexpr (std::is_same_v<T, CmdAdvance>) {
			do_advance(c.wall_dt);
		}
		else if constexpr (std::is_same_v<T, CmdSingleStep>) {
			if(!m_exp.simulations.empty()) {
				for(auto &sim : m_exp.simulations)
					sim->step();
				m_exp.sim_time = m_exp.simulations[0]->time();
				do_extract();
				publish();
			}
		}
		else if constexpr (std::is_same_v<T, CmdSetDt>) {
			for(auto &sim : m_exp.simulations)
				sim->set_dt(c.dt);
		}
		else if constexpr (std::is_same_v<T, CmdSetTimescale>) {
			double sign = c.ts < 0 ? -1.0 : 1.0;
			double mag = fabs(c.ts);
			if(mag < 1e-30) mag = fabs(m_timescale);
			if(mag < 1e-30) mag = 1e-15;
			m_timescale = sign * mag;
		}
		else if constexpr (std::is_same_v<T, CmdSetRunning>) {
			m_running = c.run;
		}
		else if constexpr (std::is_same_v<T, CmdMeasure>) {
			for(auto &sim : m_exp.simulations)
				sim->measure(c.axis);
			do_extract();
			publish();
		}
		else if constexpr (std::is_same_v<T, CmdDecohere>) {
			for(auto &sim : m_exp.simulations)
				sim->decohere(c.axis);
			do_extract();
			publish();
		}
		else if constexpr (std::is_same_v<T, CmdSetAbsorb>) {
			for(auto &sim : m_exp.simulations) {
				sim->set_absorbing_boundary(c.on);
				sim->absorb_width = c.width;
				sim->absorb_strength = c.strength;
				sim->recompute_boundary();
			}
		}
		else if constexpr (std::is_same_v<T, CmdLoad>) {
			do_load(c.lua_source);
		}
	}, cmd);
}


void SimThread::do_load(const std::string &path)
{
	linf("loading: %s", path.c_str());

	m_exp.simulations.clear();
	m_exp.setup = Setup{};

	if(!m_exp.load(path)) {
		lerr("load failed: %s", path.c_str());
		auto &st = m_state.write_buf();
		st = PublishedState{};
		st.error = "load failed";
		st.generation = ++m_generation;
		m_state.publish();
		return;
	}

	m_timescale = m_exp.timescale;
	m_running = false;
	m_generation++;

	do_extract();
	publish();

	linf("loaded: rank=%d, %zu sims", m_exp.setup.spatial_dims,
		m_exp.simulations.size());
}


void SimThread::do_advance(double wall_dt)
{
	if(!m_running || m_exp.simulations.empty()) return;

	m_exp.timescale = m_timescale;
	m_exp.running = true;
	m_exp.advance(wall_dt);

	do_extract();
	publish();
}


void SimThread::do_extract()
{
	if(m_exp.simulations.empty()) return;
	auto &sim = *m_exp.simulations[0];

	// read latest request set
	const ExtractionSet *rset = m_requests.read();

	auto &out = m_state.write_buf();
	out.n_results = 0;

	for(int i = 0; i < rset->count && i < ExtractionSet::MAX_REQUESTS; i++) {
		auto &req = rset->req[i];
		auto &res = out.results[out.n_results];

		// count free axes
		int n_axes = 0;
		for(int a = 0; a < MAX_RANK && req.axes[a] >= 0; a++)
			n_axes++;
		if(n_axes == 0) continue;

		// copy request info to result
		for(int a = 0; a < MAX_RANK; a++) res.axes[a] = req.axes[a];
		res.marginal = req.marginal;

		// fill shape
		for(int a = 0; a < n_axes; a++)
			res.shape[a] = sim.grid.axes[req.axes[a]].points;
		for(int a = n_axes; a < MAX_RANK; a++)
			res.shape[a] = -1;

		// compute total output size
		int total = 1;
		for(int a = 0; a < n_axes; a++)
			total *= res.shape[a];

		res.psi.resize(total);
		res.pot.resize(total);
		res.coherent.clear();

		if(req.marginal) {
			// |ψ|² marginal + coherent marginal ∫ψ
			res.coherent.resize(total);
			std::vector<float> marg(total);
			if(n_axes == 1)
				sim.read_marginal_1d(req.axes[0], marg.data(), res.coherent.data());
			else if(n_axes == 2)
				sim.read_marginal_2d(req.axes[0], req.axes[1], marg.data(), res.coherent.data());
			for(int j = 0; j < total; j++)
				res.psi[j] = psi_t(marg[j], 0);

			// TODO: |ψ|²-weighted potential marginal
			std::fill(res.pot.begin(), res.pot.end(), psi_t(0, 0));
		} else {
			// psi slice
			if(n_axes == 1) {
				sim.read_slice_1d(req.axes[0], req.cursor, res.psi.data());
			} else if(n_axes == 2) {
				sim.read_slice_2d(req.axes[0], req.axes[1], req.cursor,
					res.psi.data());
			}

			// TODO: potential slice extraction
			std::fill(res.pot.begin(), res.pot.end(), psi_t(0, 0));
		}

		out.n_results++;
	}
}


void SimThread::publish()
{
	if(m_exp.simulations.empty()) return;
	auto &sim = *m_exp.simulations[0];

	auto &st = m_state.write_buf();

	// scalars
	st.sim_time = m_exp.sim_time;
	st.step_count = sim.step_count;
	st.total_probability = sim.total_probability();
	st.phase_v = sim.max_potential_phase;
	st.phase_k = sim.max_kinetic_phase;
	st.dt = sim.dt;
	st.timescale = m_timescale;
	st.running = m_running;
	st.generation = m_generation;
	st.error.clear();

	for(int d = 0; d < sim.grid.rank; d++)
		st.k_nyquist_ratio[d] = sim.k_nyquist_ratio[d];

	// grid metadata
	st.grid.rank = sim.grid.rank;
	for(int d = 0; d < sim.grid.rank; d++)
		st.grid.axes[d] = sim.grid.axes[d];
	st.grid.cs = sim.cs;

	// setup metadata
	st.title = m_exp.setup.title;
	st.description = m_exp.setup.description;
	st.n_particles = (int)m_exp.setup.particles.size();

	// marginal peaks (for auto-track)
	// TODO: compute from marginals

	m_state.publish();
}
