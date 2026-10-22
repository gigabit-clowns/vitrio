// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_read_queue.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vitrio
{

/**
 * @brief A queue that puts its reads on another one, and keeps a record of
 * them.
 */
class recording_file_read_queue final
	: public file_read_queue
{
public:
	/**
	 * @brief What was asked of one read.
	 */
	struct read
	{
		std::uint64_t offset;
		std::size_t buffer_count;
		std::size_t size;
	};

	explicit recording_file_read_queue(file_read_queue &queue) noexcept
		: m_queue(queue)
		, m_outstanding(0)
	{
	}

	void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override
	{
		m_queue.queue(file, offset, buffers, tag);

		std::size_t size = 0;
		for (const auto &buffer : buffers)
		{
			size += buffer.get_size();
		}
		m_reads.push_back({offset, buffers.size(), size});
		++m_outstanding;
	}

	file_read_completion wait() override
	{
		auto completion = m_queue.wait();
		--m_outstanding;
		return completion;
	}

	const std::vector<read>& get_reads() const noexcept
	{
		return m_reads;
	}

	std::size_t get_outstanding() const noexcept
	{
		return m_outstanding;
	}

private:
	file_read_queue &m_queue;
	std::vector<read> m_reads;
	std::size_t m_outstanding;
};

} // namespace vitrio
