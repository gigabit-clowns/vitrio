// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_reader.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_reader final
	: public image_reader
{
public:
	mock_image_reader() = default;

	MAKE_CONST_MOCK0(
		get_descriptor,
		const image_descriptor&(),
		noexcept override
	);

	MAKE_CONST_MOCK0(
		get_metadata,
		const image_metadata&(),
		noexcept override
	);

	MAKE_CONST_MOCK2(
		read,
		void(array_ref destination, const image_transfer_plan &regions),
		override
	);
};

} // namespace vitrio
