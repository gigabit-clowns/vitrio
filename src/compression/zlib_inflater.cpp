// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zlib_inflater.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace vitrio
{

namespace
{

[[noreturn]]
void reject(const std::string &name, const char *reason)
{
	throw image_file_format_error(name + ": " + reason);
}

// zlib counts what it is given in a uInt, so a long stretch is handed over
// a part at a time.
uInt get_stretch(std::size_t remaining) noexcept
{
	return static_cast<uInt>(std::min<std::size_t>(
		remaining, std::numeric_limits<uInt>::max()));
}

} // anonymous namespace

zlib_inflater::zlib_inflater(
	int window_bits,
	bool multiple_members,
	std::string name
)
	: m_stream()
	, m_name(std::move(name))
	, m_multiple_members(multiple_members)
	, m_ended(false)
	, m_follows_member(false)
	, m_member_started(false)
{
	const auto status = inflateInit2(&m_stream, window_bits);
	if (status == Z_MEM_ERROR)
	{
		throw std::bad_alloc();
	}
	if (status != Z_OK)
	{
		reject(m_name, "zlib could not start inflating.");
	}
}

zlib_inflater::~zlib_inflater()
{
	inflateEnd(&m_stream);
}

bool zlib_inflater::inflate(span<const byte> &input, span<byte> &output)
{
	// zlib refuses a null buffer even with no room in it, which an empty
	// span may hold, so it is given a byte it never writes instead.
	Bytef nowhere = 0;

	while (!input.empty())
	{
		if (m_ended)
		{
			if (!m_multiple_members)
			{
				break;
			}

			// What follows the end of a member begins another one.
			if (inflateReset(&m_stream) != Z_OK)
			{
				reject(m_name, "zlib could not begin another member.");
			}
			m_ended = false;
			m_follows_member = true;
			m_member_started = false;
		}

		m_stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(input.data()));
		m_stream.avail_in = get_stretch(input.size());
		m_stream.next_out = output.empty()
			? &nowhere
			: reinterpret_cast<Bytef*>(output.data());
		m_stream.avail_out = get_stretch(output.size());
		const auto given_in = m_stream.avail_in;
		const auto given_out = m_stream.avail_out;

		const auto status = ::inflate(&m_stream, Z_NO_FLUSH);
		const std::size_t consumed = given_in - m_stream.avail_in;
		const std::size_t produced = given_out - m_stream.avail_out;
		input = make_span(
			input.data() + consumed,
			input.size() - consumed
		);
		output = make_span(
			output.data() + produced,
			output.size() - produced
		);

		if (status == Z_MEM_ERROR)
		{
			throw std::bad_alloc();
		}
		if (status == Z_DATA_ERROR && m_follows_member && !m_member_started)
		{
			// What fails to begin another member is no member at all,
			// rather than a member that is broken.
			reject(m_name, "Bytes that are not a stream follow the stream.");
		}
		if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
		{
			reject(m_name, "The stream is not one zlib inflates.");
		}

		if (consumed > 0)
		{
			m_member_started = true;
		}

		if (status == Z_STREAM_END)
		{
			m_ended = true;
			continue;
		}

		// Short of the end, zlib stops only where the input runs out or
		// the output is full.
		if (consumed == 0 && produced == 0)
		{
			break;
		}
	}

	return m_ended;
}

bool zlib_inflater::has_ended() const noexcept
{
	return m_ended;
}

const std::string& zlib_inflater::get_name() const noexcept
{
	return m_name;
}

} // namespace vitrio
