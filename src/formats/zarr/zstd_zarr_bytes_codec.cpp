// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zstd_zarr_bytes_codec.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <zstd.h>

#include <algorithm>
#include <memory>
#include <new>
#include <string>

namespace vitrio
{
namespace zarr
{

namespace
{

// What the output starts at when the frame does not say how much it holds.
const std::size_t initial_capacity = 1UL << 16;

[[noreturn]]
void reject(const std::string &reason)
{
	throw image_file_format_error("zstd_zarr_bytes_codec: " + reason);
}

struct context_deleter
{
	void operator()(ZSTD_DCtx *context) const noexcept
	{
		ZSTD_freeDCtx(context);
	}
};

} // anonymous namespace

void zstd_zarr_bytes_codec::decode(
	span<const byte> encoded,
	std::size_t limit,
	std::vector<byte> &decoded
) const
{
	const auto stated = ZSTD_getFrameContentSize(
		encoded.data(), encoded.size());
	if (stated == ZSTD_CONTENTSIZE_ERROR)
	{
		reject("The chunk is not a zstd frame.");
	}

	std::size_t capacity;
	if (stated == ZSTD_CONTENTSIZE_UNKNOWN)
	{
		capacity = std::min(
			limit, std::max(initial_capacity, encoded.size() * 4));
	}
	else if (stated > limit)
	{
		reject("The chunk decodes into more bytes than it may hold.");
	}
	else
	{
		// A frame that states its size may still be followed by another,
		// which then grows the output as one that does not would.
		capacity = static_cast<std::size_t>(stated);
	}

	const std::unique_ptr<ZSTD_DCtx, context_deleter> context(
		ZSTD_createDCtx());
	if (!context)
	{
		throw std::bad_alloc();
	}

	decoded.resize(capacity);
	ZSTD_inBuffer input = {encoded.data(), encoded.size(), 0};
	ZSTD_outBuffer output = {decoded.data(), decoded.size(), 0};
	for (;;)
	{
		const auto consumed = input.pos;
		const auto produced = output.pos;
		const auto remaining = ZSTD_decompressStream(
			context.get(), &output, &input);
		if (ZSTD_isError(remaining))
		{
			reject(
				std::string("The chunk could not be decoded: ") +
				ZSTD_getErrorName(remaining) + "."
			);
		}

		// A frame is done, flushed and the last of them.
		if (remaining == 0 && input.pos == input.size)
		{
			break;
		}

		if (output.pos == output.size)
		{
			if (decoded.size() >= limit)
			{
				reject("The chunk decodes into more bytes than it may hold.");
			}
			decoded.resize(std::min(
				limit, std::max(initial_capacity, decoded.size() * 2)));
			output.dst = decoded.data();
			output.size = decoded.size();
		}
		else if (
			input.pos == input.size ||
			(input.pos == consumed && output.pos == produced)
		)
		{
			reject("The chunk ends before its frame does.");
		}
	}

	decoded.resize(output.pos);
}

} // namespace zarr
} // namespace vitrio
