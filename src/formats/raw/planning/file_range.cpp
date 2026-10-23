// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_range.hpp"

namespace vitrio
{

file_range::file_range(std::size_t offset, std::size_t size) noexcept
	: m_offset(offset)
	, m_size(size)
{
}

std::size_t file_range::get_offset() const noexcept
{
	return m_offset;
}

std::size_t file_range::get_size() const noexcept
{
	return m_size;
}

std::size_t file_range::get_end() const noexcept
{
	return m_offset + m_size;
}

} // namespace vitrio
