#pragma once

#include <thread>
#include <atomic>

#include "simtypes.hpp"
#include "simqueue.hpp"
#include "triplebuf.hpp"
#include "experiment.hpp"

class SimThread {
public:
	SimThread();
	~SimThread();

	// lifecycle
	void start();
	void stop();

	// UI → Sim
	void push(SimCommand cmd) { m_cmds.push(std::move(cmd)); }
	void publish_requests(const ExtractionSet &set);

	// Sim → UI (always valid; swaps in new data if available)
	const PublishedState *read_state() { return m_state.read(); }

private:
	void run();
	void handle(SimCommand cmd);
	void do_load(const std::string &path);
	void do_advance(double wall_dt);
	void do_extract();
	void publish();

	// thread
	std::thread m_thread;
	std::atomic<bool> m_alive{false};

	// communication
	SimCommandQueue m_cmds;
	TripleBuffer<ExtractionSet> m_requests;
	TripleBuffer<PublishedState> m_state;

	// model state (sim-thread only)
	Experiment m_exp;
	double m_timescale{1e-15};
	bool m_running{false};
	int m_generation{0};
};
