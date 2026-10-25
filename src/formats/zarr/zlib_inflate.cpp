// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zlib_inflate.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <zlib.h>

#include <algorithm>
#include <limits>
#include <new>
#include <string>

namespace vitrio
{
namespace zarr
{

namespace
{

// What the output starts at when the stream does not say how much it holds,
// which neither kind does up front.
const std::size_t initial_capacity = 1UL << 16;

[[noreturn]]
void reject(const char *codec, const std::string &reason)
{
	throw image_file_format_error(std::string(codec) + ": " + reason);
}

// Frees the state of zlib however the inflation ends.
class inflation
{
public:
	inflation(int window_bits, const char *codec)
		: m_stream()
	{
		const auto status = inflateInit2(&m_stream, window_bits);
		if (status == Z_MEM_ERROR)
		{
			throw std::bad_alloc();
		}
		if (status != Z_OK)
		{
			reject(codec, "zlib could not start inflating.");
		}
	}

	inflation(const inflation &other) = delete;
	inflation& operator=(const inflation &other) = delete;

	~inflation()
	{
		inflateEnd(&m_stream);
	}

	z_stream& get() noexcept
	{
		return m_stream;
	}

private:
	z_stream m_stream;
};

// zlib counts what it is given in a uInt, so a long stream is handed over a
// stretch at a time.
uInt get_stretch(std::size_t remaining) noexcept
{
	return static_cast<uInt>(std::min<std::size_t>(
		remaining, std::numeric_limits<uInt>::max()));
}

} // anonymous namespace

void inflate_stream(
	span<const byte> encoded,
	int window_bits,
	std::size_t limit,
	std::vector<byte> &decoded,
	const char *codec
)
{
	inflation state(window_bits, codec);
	auto &stream = state.get();

	decoded.resize(std::min(
		limit, std::max(initial_capacity, encoded.size() * 4)));
	std::size_t consumed = 0;
	std::size_t produced = 0;

	// zlib refuses a null buffer even with no room in it, which an empty
	// vector may hand out, so it is given a byte it never writes instead.
	Bytef nowhere = 0;
	for (;;)
	{
		stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(encoded.data() + consumed));
		stream.avail_in = get_stretch(encoded.size() - consumed);
		stream.next_out = decoded.empty()
			? &nowhere
			: reinterpret_cast<Bytef*>(decoded.data() + produced);
		stream.avail_out = get_stretch(decoded.size() - produced);
		const auto given_in = stream.avail_in;
		const auto given_out = stream.avail_out;

		const auto status = inflate(&stream, Z_NO_FLUSH);
		consumed += given_in - stream.avail_in;
		produced += given_out - stream.avail_out;

		if (status == Z_STREAM_END)
		{
			break;
		}
		if (status == Z_MEM_ERROR)
		{
			throw std::bad_alloc();
		}
		if (status != Z_OK && status != Z_BUF_ERROR)
		{
			reject(codec, "The chunk is not a stream zlib inflates.");
		}

		if (produced == decoded.size())
		{
			if (decoded.size() >= limit)
			{
				reject(
					codec,
					"The chunk inflates into more bytes than it may hold."
				);
			}
			decoded.resize(std::min(limit, decoded.size() * 2));
		}
		else if (consumed == encoded.size())
		{
			reject(codec, "The chunk ends before its stream does.");
		}
	}

	if (consumed != encoded.size())
	{
		reject(codec, "The chunk goes on after its stream ends.");
	}

	decoded.resize(produced);
}

} // namespace zarr
} // namespace vitrio
