// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gzip_file_stream.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>

#include <tuning.hpp>

#include <algorithm>

namespace vitrio
{

namespace
{

// Most bytes a skip inflates at once, and so what it holds.
constexpr std::size_t discard_bytes = 1UL << 16;

std::ifstream open_file(const std::string &path)
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		throw image_file_error(
			path + ": gzip_file_stream: The file could not be opened."
		);
	}

	return file;
}

} // anonymous namespace

gzip_file_stream::gzip_file_stream(const std::string &path)
	: gzip_file_stream(path, get_gzip_read_bytes())
{
}

gzip_file_stream::gzip_file_stream(
	const std::string &path,
	std::size_t read_bytes
)
	: m_path(path)
	, m_file(open_file(path))
	, m_file_ended(false)
	, m_input(std::max<std::size_t>(read_bytes, 1))
	, m_pending()
	, m_inflater(31, true, path + ": gzip_file_stream")
	, m_position(0)
{
}

void gzip_file_stream::skip(std::uint64_t count)
{
	if (count > 0 && m_discarded.empty())
	{
		m_discarded.resize(discard_bytes);
	}

	while (count > 0)
	{
		const auto size = static_cast<std::size_t>(
			std::min<std::uint64_t>(count, m_discarded.size()));
		read(make_span(m_discarded.data(), size));
		count -= size;
	}
}

void gzip_file_stream::read(span<byte> destination)
{
	const auto requested = destination.size();

	while (!destination.empty())
	{
		if (m_pending.empty() && !m_file_ended)
		{
			read_input();
		}

		if (m_pending.empty())
		{
			// Nothing is left of the file to inflate.
			throw image_file_format_error(
				m_path + ": gzip_file_stream: " + (m_inflater.has_ended()
					? "The stream ends before the bytes asked of it."
					: "The file ends before its stream does.")
			);
		}

		const auto pending = m_pending.size();
		const auto room = destination.size();
		m_inflater.inflate(m_pending, destination);

		// An inflater of several members always takes what follows the
		// end of one, so it stops short only where one side runs out.
		if (m_pending.size() == pending && destination.size() == room)
		{
			throw image_file_format_error(
				m_path + ": gzip_file_stream: zlib inflates nothing more of "
				"the file."
			);
		}
	}

	m_position += requested;
}

std::uint64_t gzip_file_stream::get_position() const noexcept
{
	return m_position;
}

void gzip_file_stream::read_input()
{
	m_file.read(
		reinterpret_cast<char*>(m_input.data()),
		static_cast<std::streamsize>(m_input.size())
	);
	if (m_file.bad())
	{
		throw image_file_error(
			m_path + ": gzip_file_stream: The file could not be read."
		);
	}

	const auto count = static_cast<std::size_t>(m_file.gcount());
	m_file_ended = count < m_input.size();
	m_pending = make_span(
		static_cast<const byte*>(m_input.data()),
		count
	);
}

} // namespace vitrio
