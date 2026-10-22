// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_read_completion.hpp"

namespace vitrio
{

file_read_completion::file_read_completion(
	std::uint64_t tag,
	std::size_t size
) noexcept
	: m_tag(tag)
	, m_size(size)
{
}

file_read_completion::file_read_completion(
	std::uint64_t tag,
	std::error_code error
) noexcept
	: m_tag(tag)
	, m_size(0)
	, m_error(error)
{
}

std::uint64_t file_read_completion::get_tag() const noexcept
{
	return m_tag;
}

std::size_t file_read_completion::get_size() const noexcept
{
	return m_size;
}

const std::error_code& file_read_completion::get_error() const noexcept
{
	return m_error;
}

} // namespace vitrio
