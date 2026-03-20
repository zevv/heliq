#pragma once

#include <thread>
#include <atomic>
#include "simtypes.hpp"
#include "simqueue.hpp"
#include "triplebuf.hpp"

// Async simulation context. Owns model state on a dedicated thread.
// UI communicates via command queue (push) and triple-buffered state (state).
//
// Lifecycle:
//   SimContext ctx;       // thread starts
//   ctx.push(CmdLoad{…});
//   ...
//   // destructor pushes CmdStop, joins thread

class SimContext {
public:
	SimContext();
	~SimContext();

	// UI → Sim: fire-and-forget commands
	void push(SimCommand cmd) { m_cmds.push(std::move(cmd)); }

	// UI → Sim: declare extraction requests for this frame
	void request(const ExtractionRequest &r) { m_requests.find_or_insert(r); }

	// UI: call once per frame. Sends pending extraction requests to
	// sim thread and swaps in latest published state.
	void poll();

	// UI: latest published state (always valid after first poll)
	const PublishedState &state() { return m_state; }

private:
	void run();  // sim thread entry point

	SimCommandQueue m_cmds;
	ExtractionSet m_requests{};
	ExtractionSet m_prev_requests{};  // last sent, for change detection
	TripleBuffer<PublishedState> m_tbuf;
	PublishedState m_state{};  // UI-side copy, updated by poll()

	std::thread m_thread;
	std::atomic<bool> m_stop{false};

	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
