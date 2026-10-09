// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_reader_provider.hpp>

#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_reader_provider.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <trompeloeil.hpp>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"query_descriptor answers the descriptor of a file a provider serves",
	"[image_reader_provider]"
)
{
	const std::vector<std::size_t> extents = {6, 3, 5};
	const image_descriptor descriptor(
		make_span(extents),
		2,
		numerical_type::int16
	);

	const auto readers = std::make_shared<mock_image_reader_provider>();
	const auto reader = std::make_shared<mock_image_reader>();

	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));
	REQUIRE_CALL(*readers, acquire("stack.mrcs")).RETURN(reader);

	CHECK( query_descriptor(*readers, "stack.mrcs") == descriptor );
}

TEST_CASE(
	"query_descriptor reports what acquiring the file reported",
	"[image_reader_provider]"
)
{
	const auto readers = std::make_shared<mock_image_reader_provider>();

	REQUIRE_CALL(*readers, acquire("absent.mrc"))
		.SIDE_EFFECT( throw unsupported_operation_error("nothing claims it") )
		.RETURN(nullptr);

	REQUIRE_THROWS_AS(
		query_descriptor(*readers, "absent.mrc"),
		unsupported_operation_error
	);
}
