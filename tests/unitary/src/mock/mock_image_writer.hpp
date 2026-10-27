// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_writer.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <trompeloeil.hpp>

#include <memory>

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

	MAKE_MOCK3(
		write_async,
		void(
			const_array source,
			image_transfer_plan regions,
			std::shared_ptr<completion_notifier> done
		),
		override
	);

	MAKE_MOCK1(
		flush_async,
		void(std::shared_ptr<completion_notifier> done),
		override
	);
};

} // namespace vitrio
