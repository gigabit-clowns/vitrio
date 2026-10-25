// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/zarr/zarr_bytes_codec.hpp>

#include <trompeloeil.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class mock_zarr_bytes_codec final
	: public zarr::zarr_bytes_codec
{
public:
	mock_zarr_bytes_codec() = default;

	MAKE_CONST_MOCK3(
		decode,
		void(span<const byte>, std::size_t, std::vector<byte>&),
		override
	);
};

} // namespace vitrio
