#pragma once

#include "simtypes.hpp"
#include "simqueue.hpp"
#include "triplebuf.hpp"

// Async API facade for the simulation model.
//
// All widget/app access to the model goes through this class.
// The model internals (Experiment, Simulation, Solver) are hidden.
//
// PublishedState is triple-buffered. Model writes to write_buf,
// publishes via atomic swap. UI reads latest via read().
// Ready for Phase 2 (sim thread) — no API change needed.

class SimContext {
public:
	// UI → Model: fire-and-forget commands
	void push(SimCommand cmd) { m_cmds.push(std::move(cmd)); }

	// UI → Model: declare what data widgets need this frame
	void request(const ExtractionRequest &r) { m_requests.find_or_insert(r); }

	// Main loop: call once per frame. Drains commands, steps simulation,
	// fulfills extraction requests, publishes state.
	void poll(double wall_dt);

	// UI reads: latest published state (always valid after first poll)
	const PublishedState &state() { return *m_tbuf.read(); }

private:
	void handle(SimCommand &cmd);
	void extract();
	void publish();

	SimCommandQueue m_cmds;
	ExtractionSet m_requests{};
	TripleBuffer<PublishedState> m_tbuf;

	// opaque model state — only accessed by simcontext.cpp
	struct Impl;
	std::unique_ptr<Impl> m_impl;

public:
	SimContext();
	~SimContext();
};
