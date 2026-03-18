#pragma once

#include "simtypes.hpp"
#include "simqueue.hpp"

// Async API facade for the simulation model.
//
// Phase 1: synchronous. poll() executes inline on the calling thread.
// Phase 2: poll() swaps in latest state from a real sim thread.
//
// All widget/app access to the model goes through this class.
// The model internals (Experiment, Simulation, Solver) are hidden.

class Experiment;  // forward decl, private implementation detail

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
	const PublishedState &state() const { return m_state; }

private:
	void handle(SimCommand &cmd);
	void extract();
	void publish();

	SimCommandQueue m_cmds;
	ExtractionSet m_requests{};
	PublishedState m_state{};

	// opaque model state — only accessed by simcontext.cpp
	struct Impl;
	std::unique_ptr<Impl> m_impl;

public:
	SimContext();
	~SimContext();
};
