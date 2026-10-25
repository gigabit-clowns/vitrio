// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blosc_zarr_bytes_codec.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <blosc.h>

namespace vitrio
{
namespace zarr
{

void blosc_zarr_bytes_codec::decode(
	span<const byte> encoded,
	std::size_t limit,
	std::vector<byte> &decoded
) const
{
	// The header says how many bytes the chunk decodes into, and the
	// validation checks it against what the chunk holds, before anything is
	// allocated for it.
	std::size_t size = 0;
	if (blosc_cbuffer_validate(encoded.data(), encoded.size(), &size) < 0)
	{
		throw image_file_format_error(
			"blosc_zarr_bytes_codec: The chunk is not a blosc buffer."
		);
	}
	if (size > limit)
	{
		throw image_file_format_error(
			"blosc_zarr_bytes_codec: The chunk decodes into more bytes than "
			"it may hold."
		);
	}

	decoded.resize(size);
	if (size == 0)
	{
		return;
	}

	// The context of a call of its own, so that calls on many threads at
	// once share nothing, and no threads of blosc's own.
	const auto written =
		blosc_decompress_ctx(encoded.data(), decoded.data(), size, 1);
	if (written < 0 || static_cast<std::size_t>(written) != size)
	{
		throw image_file_format_error(
			"blosc_zarr_bytes_codec: The chunk could not be decoded."
		);
	}
}

} // namespace zarr
} // namespace vitrio
