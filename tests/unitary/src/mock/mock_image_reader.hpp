// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_reader.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <trompeloeil.hpp>

#include <memory>

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

	MAKE_CONST_MOCK3(
		read_async,
		void(
			array destination,
			image_transfer_plan regions,
			std::shared_ptr<completion_notifier> done
		),
		override
	);
};

} // namespace vitrio
