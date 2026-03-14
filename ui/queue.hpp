
#include <vector>
#include <condition_variable>
#include <mutex>

template <typename T>
class Queue {

public:

	Queue(size_t capacity)
		: m_head(0)
	    , m_tail(0)
		, m_data(capacity)
	{
	}

	void push(const T& val)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while(m_head - m_tail == m_data.size()) {
			m_cv.wait(lock);
		}
		m_data[m_head % m_data.size()] = val;
		m_head ++;
		m_cv.notify_all();
	}

	bool pop(T& val, bool block)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while(m_head == m_tail) {
			if(!block) {
				return false;
			}
			m_cv.wait(lock);
		}
		val = m_data[m_tail % m_data.size()];
		m_tail ++;
		m_cv.notify_all();
		return true;
	}

	size_t used()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_head - m_tail;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	size_t m_head;
	size_t m_tail;
	std::vector<T> m_data;
};
