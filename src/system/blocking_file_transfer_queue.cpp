// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blocking_file_transfer_queue.hpp"

#include "file_sync.hpp"
#include "scattered_file_read.hpp"
#include "scattered_file_write.hpp"

#include <assert.hpp>

#include <system_error>

namespace vitrio
{

blocking_file_transfer_queue::blocking_file_transfer_queue() = default;
blocking_file_transfer_queue::~blocking_file_transfer_queue() = default;

void blocking_file_transfer_queue::queue_read(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::uint64_t tag
)
{
	std::error_code error;
	const auto size = read_scattered(file, offset, buffers, error);
	complete(tag, size, error);
}

void blocking_file_transfer_queue::queue_write(
	file_handle_t file,
	std::uint64_t offset,
	span<const const_memory_range> buffers,
	std::uint64_t tag
)
{
	std::error_code error;
	const auto size = write_scattered(file, offset, buffers, error);
	complete(tag, size, error);
}

void blocking_file_transfer_queue::queue_sync(
	file_handle_t file,
	std::uint64_t tag
)
{
	std::error_code error;
	sync_file(file, error);
	complete(tag, 0, error);
}

void blocking_file_transfer_queue::complete(
	std::uint64_t tag,
	std::size_t size,
	std::error_code error
)
{
	if (error)
	{
		m_completions.emplace_back(tag, error);
	}
	else
	{
		m_completions.emplace_back(tag, size);
	}
}

file_transfer_completion blocking_file_transfer_queue::wait()
{
	VITRIO_ASSERT(!m_completions.empty());

	const auto completion = m_completions.front();
	m_completions.pop_front();
	return completion;
}

} // namespace vitrio
