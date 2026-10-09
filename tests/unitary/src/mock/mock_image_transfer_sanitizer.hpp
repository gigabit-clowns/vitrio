// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_transfer_sanitizer.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <trompeloeil.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class mock_image_transfer_sanitizer final
	: public image_transfer_sanitizer
{
public:
	mock_image_transfer_sanitizer() = default;

	MAKE_CONST_MOCK3(
		sanitize,
		std::vector<image_transfer_plan>(
			const image_transfer_plan &regions,
			span<const std::size_t> file_extents,
			span<const std::size_t> array_extents
		),
		override
	);
};

} // namespace vitrio
