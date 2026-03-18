#pragma once

#include "simtypes.hpp"
#include "simqueue.hpp"
#include "experiment.hpp"

// Async API facade for the simulation model.
//
// Phase 1: synchronous. poll() executes inline on the calling thread.
// Phase 2: poll() swaps in latest state from a real sim thread.
//
// All widget/app access to the model goes through this class.

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

	// transitional: direct access to experiment during migration
	// TODO: remove once all widgets read from state()
	Experiment &experiment() { return m_exp; }

private:
	void handle(SimCommand &cmd);
	void extract();
	void publish();

	SimCommandQueue m_cmds;
	ExtractionSet m_requests{};
	PublishedState m_state{};
	Experiment m_exp{};
	int m_generation{};
};
