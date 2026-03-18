#pragma once

#include <mutex>
#include <condition_variable>
#include <vector>
#include "simtypes.hpp"

// Thread-safe command queue (UI → Sim).
// UI pushes commands, sim thread drains them in batches.
// Condvar wakes sim thread when idle.

class SimCommandQueue {
public:
	void push(SimCommand cmd) {
		{
			std::lock_guard<std::mutex> lk(m_mtx);
			m_queue.push_back(std::move(cmd));
		}
		m_cv.notify_one();
	}

	// drain all pending commands, call fn for each
	template<typename Fn>
	void drain(Fn fn) {
		std::vector<SimCommand> batch;
		{
			std::lock_guard<std::mutex> lk(m_mtx);
			batch.swap(m_queue);
		}
		for(auto &cmd : batch)
			fn(std::move(cmd));
	}

	// wait until command arrives or timeout (for idle sim thread)
	void wait() {
		std::unique_lock<std::mutex> lk(m_mtx);
		m_cv.wait(lk, [&]{ return !m_queue.empty(); });
	}

	void wait_for(std::chrono::milliseconds ms) {
		std::unique_lock<std::mutex> lk(m_mtx);
		m_cv.wait_for(lk, ms, [&]{ return !m_queue.empty(); });
	}

	void wake() { m_cv.notify_one(); }

private:
	std::mutex m_mtx;
	std::condition_variable m_cv;
	std::vector<SimCommand> m_queue;
};
