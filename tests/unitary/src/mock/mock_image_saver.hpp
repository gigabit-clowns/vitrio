// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_saver.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_sanitizer.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_saver final
	: public image_saver
{
public:
	mock_image_saver() = default;

	MAKE_CONST_MOCK3(
		save,
		std::shared_ptr<completion>(
			const_array source,
			const image_transaction_plan &plan,
			std::shared_ptr<const image_transfer_sanitizer> sanitizer
		),
		override
	);
};

} // namespace vitrio
