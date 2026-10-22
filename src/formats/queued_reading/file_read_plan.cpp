// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_read_plan.hpp"

#include <assert.hpp>

namespace vitrio
{

file_read_plan::file_read_plan() noexcept = default;

void file_read_plan::add_read(
	std::size_t step,
	std::uint64_t offset,
	span<const memory_range> buffers
)
{
	VITRIO_ASSERT(m_reads.empty() || m_reads.back().step <= step);

	std::size_t size = 0;
	for (const auto &buffer : buffers)
	{
		size += buffer.get_size();
	}

	m_reads.push_back({step, offset, size, m_buffers.size(), buffers.size()});
	m_buffers.insert(m_buffers.end(), buffers.begin(), buffers.end());
}

std::size_t file_read_plan::get_step_count() const noexcept
{
	return m_reads.empty() ? 0 : m_reads.back().step + 1;
}

std::size_t file_read_plan::get_read_count() const noexcept
{
	return m_reads.size();
}

std::size_t file_read_plan::get_step(std::size_t read) const noexcept
{
	VITRIO_ASSERT(read < m_reads.size());
	return m_reads[read].step;
}

std::uint64_t file_read_plan::get_offset(std::size_t read) const noexcept
{
	VITRIO_ASSERT(read < m_reads.size());
	return m_reads[read].offset;
}

std::size_t file_read_plan::get_size(std::size_t read) const noexcept
{
	VITRIO_ASSERT(read < m_reads.size());
	return m_reads[read].size;
}

span<const memory_range>
file_read_plan::get_buffers(std::size_t read) const noexcept
{
	VITRIO_ASSERT(read < m_reads.size());
	const auto &planned = m_reads[read];
	return make_span(
		m_buffers.data() + planned.first_buffer,
		planned.buffer_count
	);
}

} // namespace vitrio
