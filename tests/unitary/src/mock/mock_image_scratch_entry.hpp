// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_scratch_entry.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <trompeloeil.hpp>

#include <memory>

namespace vitrio
{

class mock_image_scratch_entry final
	: public image_scratch_entry
{
public:
	mock_image_scratch_entry() = default;

	MAKE_CONST_MOCK2(
		read,
		image_transfer_plan(
			array_ref destination,
			const image_transfer_plan &regions
		),
		override
	);

	MAKE_MOCK3(
		store_async,
		void(
			const image_reader &file,
			image_transfer_plan regions,
			std::shared_ptr<completion_notifier> done
		),
		override
	);
};

} // namespace vitrio
