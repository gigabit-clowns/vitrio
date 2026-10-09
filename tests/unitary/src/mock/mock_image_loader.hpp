// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_loader.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_sanitizer.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_loader final
	: public image_loader
{
public:
	mock_image_loader() = default;

	MAKE_CONST_MOCK3(
		load,
		std::shared_ptr<completion>(
			array destination,
			const image_transaction_plan &plan,
			std::shared_ptr<const image_transfer_sanitizer> sanitizer
		),
		override
	);
};

} // namespace vitrio
