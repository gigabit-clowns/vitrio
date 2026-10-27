// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zlib_inflate.hpp"
#include "zlib_inflater.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <algorithm>
#include <string>

namespace vitrio
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

} // anonymous namespace

void inflate_stream(
	span<const byte> encoded,
	int window_bits,
	std::size_t limit,
	std::vector<byte> &decoded,
	const char *codec
)
{
	zlib_inflater inflater(window_bits, false, codec);

	decoded.resize(std::min(
		limit, std::max(initial_capacity, encoded.size() * 4)));
	auto input = encoded;
	std::size_t produced = 0;

	for (;;)
	{
		auto output = make_span(
			decoded.data() + produced,
			decoded.size() - produced
		);
		const auto room = output.size();
		const auto ended = inflater.inflate(input, output);
		produced += room - output.size();

		if (ended)
		{
			break;
		}

		if (output.empty())
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
		else
		{
			reject(codec, "The chunk ends before its stream does.");
		}
	}

	if (!input.empty())
	{
		reject(codec, "The chunk goes on after its stream ends.");
	}

	decoded.resize(produced);
}

} // namespace vitrio
