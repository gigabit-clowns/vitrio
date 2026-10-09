// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_writer.hpp>

#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_writer final
	: public image_writer
{
public:
	mock_image_writer() = default;

	MAKE_CONST_MOCK0(
		get_descriptor,
		const image_descriptor&(),
		noexcept override
	);

	MAKE_MOCK2(
		write,
		void(const_array_ref source, const image_transfer_plan &regions),
		override
	);

	MAKE_MOCK0(flush, void(), override);
};

} // namespace vitrio
