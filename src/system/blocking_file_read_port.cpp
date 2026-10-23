// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blocking_file_read_port.hpp"

namespace vitrio
{

blocking_file_read_port::blocking_file_read_port()
	: m_outstanding(0)
	, m_woken(false)
{
}

blocking_file_read_port::~blocking_file_read_port() = default;

void blocking_file_read_port::queue(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::uint64_t tag
)
{
	m_queue.queue(file, offset, buffers, tag);
	++m_outstanding;
}

void blocking_file_read_port::wake() noexcept
{
	// Notified under the lock, so that the port may be destroyed as soon as
	// its waiter has seen the flag.
	const std::lock_guard<std::mutex> lock(m_mutex);
	m_woken = true;
	m_woken_condition.notify_one();
}

boost::optional<file_read_completion> blocking_file_read_port::wait()
{
	// Each read was carried out as it was queued, so its completion is
	// returned without waiting.
	if (m_outstanding > 0)
	{
		--m_outstanding;
		return m_queue.wait();
	}

	std::unique_lock<std::mutex> lock(m_mutex);
	m_woken_condition.wait(lock, [this] { return m_woken; });
	m_woken = false;
	return boost::none;
}

} // namespace vitrio
