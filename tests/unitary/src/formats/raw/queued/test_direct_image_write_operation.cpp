// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/direct_image_write_operation.hpp>

#include "../../../mock/mock_file_transfer_scope.hpp"

#include <formats/raw/create_image_file.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_write_walk.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// A stack of six images of two rows of three float32 values, behind a
// preamble of 16 bytes.
const std::vector<std::size_t> stack_extents = {6, 2, 3};
const std::size_t image_size = 6 * sizeof(float);

raw_image_layout make_layout()
{
	return raw_image_layout(
		image_descriptor(make_span(stack_extents), 2, numerical_type::float32),
		std::vector<std::ptrdiff_t>{6, 3, 1},
		16,
		get_system_byte_order()
	);
}

// An operation that writes the slots of a batch as images 1, 2 and 4.
std::unique_ptr<direct_image_write_operation> make_operation(
	const std::string &path
)
{
	const auto layout = make_layout();
	create_image_file(path, 16 + layout.get_data_size());

	image_transfer_plan regions(image_transfer_shape({2, 3}, 3, 3));
	const std::vector<std::size_t> images = {1, 2, 4};
	for (std::size_t slot = 0; slot < images.size(); ++slot)
	{
		regions.add(
			make_span(std::vector<std::size_t>{images[slot], 0, 0}),
			make_span(std::vector<std::size_t>{slot, 0, 0})
		);
	}

	const auto source =
		make_host_array<float>({3, 2, 3}, numerical_type::float32);
	const auto &origin = source.get_descriptor();
	const image_region_write_walk walk(
		regions,
		layout.get_descriptor().get_extents(),
		layout.get_strides(),
		origin.get_extents(),
		origin.get_strides(),
		origin.get_offset()
	);

	return std::make_unique<direct_image_write_operation>(
		std::make_shared<writable_image_file>(path),
		layout,
		source.share_const(),
		walk
	);
}

} // anonymous namespace

TEST_CASE(
	"a direct_image_write_operation sends every write as it starts, the "
	"regions that follow one another in the file as one",
	"[direct_image_write_operation]"
)
{
	const scoped_path path("direct_image_write_operation_start.raw");
	const auto operation = make_operation(path.get());

	mock_file_transfer_scope scope;
	REQUIRE_CALL(
		scope,
		queue_write(trompeloeil::_, 16 + image_size, trompeloeil::_, 0u)
	)
		.WITH( _3.size() == 2 );
	REQUIRE_CALL(
		scope,
		queue_write(trompeloeil::_, 16 + 4 * image_size, trompeloeil::_, 1u)
	)
		.WITH( _3.size() == 1 );

	operation->start(scope);
}

TEST_CASE(
	"a direct_image_write_operation is done with each write that completes, "
	"and fails with one the system fails",
	"[direct_image_write_operation]"
)
{
	const scoped_path path("direct_image_write_operation_completions.raw");
	const auto operation = make_operation(path.get());

	mock_file_transfer_scope scope;
	ALLOW_CALL(
		scope,
		queue_write(
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_
		)
	);
	FORBID_CALL(scope, run(trompeloeil::_, trompeloeil::_));
	operation->start(scope);

	CHECK_NOTHROW(
		operation->on_transfer(
			file_transfer_completion(0, 2 * image_size),
			scope
		)
	);
	REQUIRE_THROWS_MATCHES(
		operation->on_transfer(
			file_transfer_completion(
				1,
				std::make_error_code(std::errc::no_space_on_device)
			),
			scope
		),
		image_file_error,
		Catch::Matchers::MessageMatches(
			Catch::Matchers::StartsWith(path.get() + ": ")
		)
	);
}
