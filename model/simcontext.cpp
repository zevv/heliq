#include "simcontext.hpp"
#include "experiment.hpp"
#include <SDL3/SDL_events.h>
#include <cstdio>
#include <chrono>
#include <memory>

// SDL user event to wake main loop when new state is published
static Uint32 g_wake_event = 0;

static void push_wake_event()
{
	if(!g_wake_event) return;
	SDL_Event ev{};
	ev.type = g_wake_event;
	SDL_PushEvent(&ev);
}


struct SimContext::Impl {
	Experiment exp{};
	int generation{};
	int last_published_gen{-1};
	PublishedState st{};
	ExtractionSet requests{};

	void extract();
	void publish();
};


SimContext::SimContext() : m_impl(std::make_unique<Impl>())
{
	if(!g_wake_event)
		g_wake_event = SDL_RegisterEvents(1);
	m_thread = std::thread([this]{ run(); });
}


SimContext::~SimContext()
{
	m_cmds.push(CmdStop{});
	m_cmds.wake();
	if(m_thread.joinable())
		m_thread.join();
}


void SimContext::poll()
{
	// send extraction requests only when they changed
	if(!(m_requests == m_prev_requests)) {
		m_cmds.push(CmdExtract{m_requests});
		m_prev_requests = m_requests;
	}
	m_requests = {};

	// swap in latest published state
	auto *p = m_tbuf.read();
	if(p) m_state = *p;
}


// --- sim thread ---

void SimContext::run()
{
	using clock = std::chrono::steady_clock;
	auto prev = clock::now();

	while(!m_stop.load(std::memory_order_relaxed)) {
		auto &exp = m_impl->exp;
		auto &st = m_impl->st;

		// drain commands
		bool did_drain = false;
		m_cmds.drain([&](SimCommand cmd) {
			did_drain = true;
			std::visit([&](auto &c) {
				using T = std::decay_t<decltype(c)>;

				if constexpr (std::is_same_v<T, CmdStop>) {
					m_stop.store(true, std::memory_order_relaxed);
				}
				else if constexpr (std::is_same_v<T, CmdLoad>) {
					if(exp.load(std::move(c.setup))) {
						m_impl->generation++;
					} else {
						st.error = "load failed";
					}
					st.running = false;
					st.generation = m_impl->generation;
				}
				else if constexpr (std::is_same_v<T, CmdAdvance>) {
					// ignored — sim thread uses its own wall clock
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
					double sign = c.ts < 0 ? -1.0 : 1.0;
					double mag = fabs(c.ts);
					if(mag < 1e-30) mag = fabs(exp.timescale);
					if(mag < 1e-30) mag = 1e-15;
					exp.timescale = sign * mag;
				}
				else if constexpr (std::is_same_v<T, CmdSetRunning>) {
					st.running = c.run;
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
				else if constexpr (std::is_same_v<T, CmdExtract>) {
					m_impl->requests = c.requests;
				}
			}, cmd);
		});

		if(m_stop.load(std::memory_order_relaxed)) break;

		// advance using sim thread's own wall clock
		bool did_work = false;
		{
			auto now = clock::now();
			double wdt = std::chrono::duration<double>(now - prev).count();
			prev = now;
			if(wdt > 0.1) wdt = 1.0 / 60.0;  // clamp on resume from sleep
			if(st.running && !exp.simulations.empty()) {
				exp.running = true;
				size_t before = exp.simulations[0]->step_count;
				exp.advance(wdt);
				did_work = (exp.simulations[0]->step_count != before);
			}
		}

		// extract + publish only when state changed
		if(did_work || did_drain) {
			if(!exp.simulations.empty()) {
				m_impl->extract();
				m_impl->publish();
				m_tbuf.write_buf() = m_impl->st;
				m_tbuf.publish();
				push_wake_event();
			}
		}

		// pace: running → yield briefly; idle → block until command arrives
		if(st.running) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} else {
			prev = clock::now();  // reset clock so next wake doesn't see huge dt
			m_cmds.wait();
		}
	}
}


// --- extraction (runs on sim thread) ---

void SimContext::Impl::extract()
{
	auto &sim = *exp.simulations[0];
	st.n_results = 0;

	for(int i = 0; i < requests.count; i++) {
		auto &req = requests.req[i];
		auto &res = st.results[st.n_results];

		int n_axes = 0;
		for(int a = 0; a < MAX_RANK && req.axes[a] >= 0; a++)
			n_axes++;
		if(n_axes == 0) continue;

		for(int a = 0; a < MAX_RANK; a++) res.axes[a] = req.axes[a];
		res.marginal = req.marginal;

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
			res.coherent.resize(total);
			std::vector<float> marg(total);
			if(n_axes == 1)
				sim.read_marginal_1d(req.axes[0], marg.data(), res.coherent.data());
			else if(n_axes == 2)
				sim.read_marginal_2d(req.axes[0], req.axes[1], marg.data(), res.coherent.data());
			for(int j = 0; j < total; j++)
				res.psi[j] = psi_t(marg[j], 0);

			std::fill(res.pot.begin(), res.pot.end(), psi_t(0, 0));
			{
				const psi_t *V = sim.potential.data();
				int out_strides[MAX_RANK] = {};
				if(n_axes >= 1) out_strides[n_axes - 1] = 1;
				for(int a = n_axes - 2; a >= 0; a--)
					out_strides[a] = out_strides[a + 1] * res.shape[a + 1];

				sim.grid.each([&](size_t idx, const int *coords, const double *) {
					int oi = 0;
					for(int a = 0; a < n_axes; a++)
						oi += coords[req.axes[a]] * out_strides[a];
					float v = std::abs(V[idx].real());
					float cur = res.pot[oi].real();
					if(v > cur) res.pot[oi] = psi_t(v, 0);
				});
			}
		} else {
			if(n_axes == 1)
				sim.read_slice_1d(req.axes[0], req.cursor, res.psi.data());
			else if(n_axes == 2)
				sim.read_slice_2d(req.axes[0], req.axes[1], req.cursor,
					res.psi.data());

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

		st.n_results++;
	}
}


void SimContext::Impl::publish()
{
	auto &sim = *exp.simulations[0];

	st.sim_time = exp.sim_time;
	st.step_count = sim.step_count;
	st.total_probability = sim.total_probability();
	st.phase_v = sim.max_potential_phase;
	st.phase_k = sim.max_kinetic_phase;
	st.dt = sim.dt;
	st.timescale = exp.timescale;
	st.generation = generation;
	st.error.clear();

	for(int d = 0; d < sim.grid.rank; d++)
		st.k_nyquist_ratio[d] = sim.k_nyquist_ratio[d];

	st.grid.rank = sim.grid.rank;
	for(int d = 0; d < sim.grid.rank; d++)
		st.grid.axes[d] = sim.grid.axes[d];
	st.grid.cs = sim.cs;

	if(st.generation != last_published_gen) {
		st.setup = exp.setup;
		last_published_gen = st.generation;
	}

	st.absorbing_boundary = sim.absorbing_boundary;
	st.absorb_width = sim.absorb_width;
	st.absorb_strength = sim.absorb_strength;
}
