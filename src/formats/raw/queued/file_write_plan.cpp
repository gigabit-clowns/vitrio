// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_write_plan.hpp"

#include <vitrio/exceptions/image_file_error.hpp>

#include <assert.hpp>

namespace vitrio
{

file_write_plan::file_write_plan() noexcept = default;

void file_write_plan::add_group()
{
	m_first_writes.push_back(m_writes.size());
}

void file_write_plan::add_write(
	std::uint64_t offset,
	span<const const_memory_range> buffers
)
{
	VITRIO_ASSERT(!m_first_writes.empty());

	std::size_t size = 0;
	for (const auto &buffer : buffers)
	{
		size += buffer.get_size();
	}

	m_writes.push_back({
		m_first_writes.size() - 1,
		offset,
		size,
		m_buffers.size(),
		buffers.size()
	});
	m_buffers.insert(m_buffers.end(), buffers.begin(), buffers.end());
}

std::size_t file_write_plan::get_group_count() const noexcept
{
	return m_first_writes.size();
}

std::size_t file_write_plan::get_first_write(std::size_t group) const noexcept
{
	VITRIO_ASSERT(group < m_first_writes.size());
	return m_first_writes[group];
}

std::size_t file_write_plan::get_write_count(std::size_t group) const noexcept
{
	VITRIO_ASSERT(group < m_first_writes.size());
	const auto end = group + 1 < m_first_writes.size()
		? m_first_writes[group + 1]
		: m_writes.size();
	return end - m_first_writes[group];
}

std::size_t file_write_plan::get_write_count() const noexcept
{
	return m_writes.size();
}

std::size_t file_write_plan::get_group(std::size_t write) const noexcept
{
	VITRIO_ASSERT(write < m_writes.size());
	return m_writes[write].group;
}

std::uint64_t file_write_plan::get_offset(std::size_t write) const noexcept
{
	VITRIO_ASSERT(write < m_writes.size());
	return m_writes[write].offset;
}

std::size_t file_write_plan::get_size(std::size_t write) const noexcept
{
	VITRIO_ASSERT(write < m_writes.size());
	return m_writes[write].size;
}

span<const const_memory_range>
file_write_plan::get_buffers(std::size_t write) const noexcept
{
	VITRIO_ASSERT(write < m_writes.size());
	const auto &planned = m_writes[write];
	return make_span(
		m_buffers.data() + planned.first_buffer,
		planned.buffer_count
	);
}

std::size_t take_write(
	const std::string &path,
	const file_write_plan &plan,
	const file_transfer_completion &completion
)
{
	const auto write = static_cast<std::size_t>(completion.get_tag());
	VITRIO_ASSERT(write < plan.get_write_count());

	if (completion.get_error())
	{
		throw image_file_error(
			path + ": file_write_plan: The file could not be written: " +
			completion.get_error().message()
		);
	}

	// A write that succeeded wrote every byte of its buffers.
	VITRIO_ASSERT(completion.get_size() == plan.get_size(write));
	return write;
}

} // namespace vitrio
