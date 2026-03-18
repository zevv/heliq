#pragma once

#include <atomic>
#include <array>

// Lock-free triple buffer. Producer writes, consumer reads latest.
// Producer never waits. Consumer never waits. At most one result
// is silently dropped if producer is faster than consumer.
//
// Usage:
//   Producer: T &buf = tb.write_buf(); fill(buf); tb.publish();
//   Consumer: const T *p = tb.read();  // nullptr if nothing new

template<typename T>
class TripleBuffer {
public:
	// get writable buffer (producer only, single thread)
	T &write_buf() { return m_buf[m_write]; }

	// publish current write buffer (producer only)
	void publish() {
		int w = m_write;
		int m = m_mid.exchange(w, std::memory_order_acq_rel);
		m_write = m;  // reclaim old mid as new write buffer
		m_fresh.store(true, std::memory_order_release);
	}

	// read latest published buffer (consumer only, single thread)
	// always returns valid pointer; swaps in new data if available
	const T *read() {
		if(m_fresh.load(std::memory_order_acquire)) {
			int m = m_mid.exchange(m_read, std::memory_order_acq_rel);
			m_read = m;
			m_fresh.store(false, std::memory_order_release);
		}
		return &m_buf[m_read];
	}

	// true if producer published since last read()
	bool fresh() const { return m_fresh.load(std::memory_order_acquire); }

private:
	std::array<T, 3> m_buf{};
	int m_write{0};
	int m_read{1};
	std::atomic<int> m_mid{2};
	std::atomic<bool> m_fresh{false};
};
