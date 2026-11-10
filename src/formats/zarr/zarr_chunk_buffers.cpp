// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_chunk_buffers.hpp"

#include <iterator>
#include <utility>

namespace vitrio
{
namespace zarr
{

zarr_chunk_buffers::zarr_chunk_buffers(std::size_t byte_budget) noexcept
	: m_bytes(0)
	, m_byte_budget(byte_budget)
{
}

std::vector<byte> zarr_chunk_buffers::take(std::size_t size)
{
	std::vector<byte> buffer;
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		for (auto kept = m_buffers.rbegin(); kept != m_buffers.rend(); ++kept)
		{
			if (kept->capacity() >= size)
			{
				buffer = std::move(*kept);
				m_buffers.erase(std::next(kept).base());
				m_bytes -= buffer.capacity();
				break;
			}
		}
	}

	// Within its memory, sizing fills nothing.
	buffer.resize(size);
	return buffer;
}

void zarr_chunk_buffers::give(std::vector<byte> buffer) noexcept
{
	const auto bytes = buffer.capacity();
	if (bytes == 0)
	{
		return;
	}

	// Released with the lock held: the memory of a buffer is given back to
	// the system at once, which takes a while for a large one, but what is
	// released has no reason to be the memory any other thread is waiting
	// for.
	try
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (bytes <= m_byte_budget - m_bytes)
		{
			m_buffers.push_back(std::move(buffer));
			m_bytes += bytes;
		}
	}
	catch (...)
	{
		// Out of memory for the list: the buffer is released instead.
	}
}

std::size_t zarr_chunk_buffers::get_kept_bytes() const
{
	const std::lock_guard<std::mutex> lock(m_mutex);
	return m_bytes;
}

} // namespace zarr
} // namespace vitrio
