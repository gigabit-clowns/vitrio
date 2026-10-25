// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_scratch_storage.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <trompeloeil.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

class mock_image_scratch_storage final
	: public image_scratch_storage
{
public:
	mock_image_scratch_storage() = default;

	MAKE_CONST_MOCK0(get_size, std::size_t(), noexcept override);

	MAKE_CONST_MOCK2(
		read,
		void(std::size_t offset, span<byte> bytes),
		override
	);

	MAKE_MOCK2(
		write,
		void(std::size_t offset, span<const byte> bytes),
		override
	);

	MAKE_MOCK3(
		write_async,
		void(
			std::size_t offset,
			span<const byte> bytes,
			std::shared_ptr<completion_notifier> done
		),
		override
	);

	MAKE_CONST_MOCK4(
		serve_async,
		void(
			const array_descriptor &values,
			array destination,
			image_transfer_plan regions,
			std::shared_ptr<completion_notifier> done
		),
		override
	);

	MAKE_MOCK4(
		load_async,
		void(
			std::shared_ptr<const image_reader> file,
			const array_descriptor &values,
			image_transfer_plan regions,
			std::shared_ptr<completion_notifier> done
		),
		override
	);
};

} // namespace vitrio
