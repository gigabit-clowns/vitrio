// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zlib_deflate.hpp"

#include <zlib.h>

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace vitrio
{
namespace zarr
{

namespace
{

[[noreturn]]
void fail(const char *codec, const std::string &reason)
{
	throw std::runtime_error(std::string(codec) + ": " + reason);
}

// Frees the state of zlib however the deflation ends.
class deflation
{
public:
	deflation(int window_bits, int level, const char *codec)
		: m_stream()
	{
		const auto status = deflateInit2(
			&m_stream, level, Z_DEFLATED, window_bits, 8,
			Z_DEFAULT_STRATEGY);
		if (status == Z_MEM_ERROR)
		{
			throw std::bad_alloc();
		}
		if (status != Z_OK)
		{
			fail(codec, "zlib could not start deflating.");
		}
	}

	deflation(const deflation &other) = delete;
	deflation& operator=(const deflation &other) = delete;

	~deflation()
	{
		deflateEnd(&m_stream);
	}

	z_stream& get() noexcept
	{
		return m_stream;
	}

private:
	z_stream m_stream;
};

uInt get_stretch(std::size_t remaining) noexcept
{
	return static_cast<uInt>(std::min<std::size_t>(
		remaining, std::numeric_limits<uInt>::max()));
}

} // anonymous namespace

void deflate_stream(
	span<const byte> decoded,
	int window_bits,
	int level,
	std::vector<byte> &encoded,
	const char *codec
)
{
	deflation state(window_bits, level, codec);
	auto &stream = state.get();

	// What deflateBound says holds the stream whole, plus room for the
	// header of gzip, which it does not count.
	encoded.resize(static_cast<std::size_t>(deflateBound(
		&stream, static_cast<uLong>(decoded.size()))) + 64);

	std::size_t consumed = 0;
	std::size_t produced = 0;
	for (;;)
	{
		stream.next_in = const_cast<Bytef*>(
			reinterpret_cast<const Bytef*>(decoded.data() + consumed));
		stream.avail_in = get_stretch(decoded.size() - consumed);
		stream.next_out = reinterpret_cast<Bytef*>(encoded.data() + produced);
		stream.avail_out = get_stretch(encoded.size() - produced);
		const auto given_in = stream.avail_in;
		const auto given_out = stream.avail_out;

		const auto last = consumed + given_in == decoded.size();
		const auto status = deflate(&stream, last ? Z_FINISH : Z_NO_FLUSH);
		consumed += given_in - stream.avail_in;
		produced += given_out - stream.avail_out;

		if (status == Z_STREAM_END)
		{
			break;
		}
		if (status != Z_OK && status != Z_BUF_ERROR)
		{
			fail(codec, "zlib could not deflate the chunk.");
		}
		if (produced == encoded.size())
		{
			encoded.resize(encoded.size() * 2);
		}
	}

	encoded.resize(produced);
}

} // namespace zarr
} // namespace vitrio
