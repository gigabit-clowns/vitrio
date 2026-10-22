// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blocking_file_read_queue.hpp"

#include "scattered_file_read.hpp"

#include <assert.hpp>

#include <system_error>

namespace vitrio
{

blocking_file_read_queue::blocking_file_read_queue() = default;
blocking_file_read_queue::~blocking_file_read_queue() = default;

void blocking_file_read_queue::queue(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::uint64_t tag
)
{
	std::error_code error;
	const auto size = read_scattered(file, offset, buffers, error);
	if (error)
	{
		m_completions.emplace_back(tag, error);
	}
	else
	{
		m_completions.emplace_back(tag, size);
	}
}

file_read_completion blocking_file_read_queue::wait()
{
	VITRIO_ASSERT(!m_completions.empty());

	const auto completion = m_completions.front();
	m_completions.pop_front();
	return completion;
}

} // namespace vitrio
