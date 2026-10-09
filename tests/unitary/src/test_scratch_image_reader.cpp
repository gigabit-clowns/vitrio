// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <scratch_image_reader.hpp>

#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_scratch_entry.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <trompeloeil.hpp>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

const std::vector<std::size_t> image_extents = {4, 4};
const std::vector<std::size_t> batch_extents = {3, 4, 4};

array make_test_array()
{
	return make_array(
		make_contiguous_array_descriptor(
			make_span(batch_extents),
			numerical_type::float32
		)
	);
}

// Image `indices[i]` of a stack, landing in slot `slots[i]` of a batch.
image_transfer_plan make_plan(
	const std::vector<std::size_t> &indices,
	const std::vector<std::size_t> &slots
)
{
	image_transfer_plan plan(image_transfer_shape(image_extents, 3, 3));
	for (std::size_t i = 0; i < indices.size(); ++i)
	{
		const std::array<std::size_t, 3> file_offset = {indices[i], 0, 0};
		const std::array<std::size_t, 3> array_offset = {slots[i], 0, 0};
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

std::vector<std::size_t> to_vector(span<const std::size_t> values)
{
	return std::vector<std::size_t>(values.begin(), values.end());
}

bool same_regions(
	const image_transfer_plan &lhs,
	const image_transfer_plan &rhs
)
{
	if (lhs.get_region_count() != rhs.get_region_count())
	{
		return false;
	}

	for (std::size_t i = 0; i < lhs.get_region_count(); ++i)
	{
		const auto same =
			to_vector(lhs.get_file_offset(i)) ==
				to_vector(rhs.get_file_offset(i)) &&
			to_vector(lhs.get_array_offset(i)) ==
				to_vector(rhs.get_array_offset(i));
		if (!same)
		{
			return false;
		}
	}

	return true;
}

} // anonymous namespace

TEST_CASE(
	"a scratch_image_reader reports what the reader over its file reports",
	"[scratch_image_reader]"
)
{
	const std::vector<std::size_t> extents = {8, 4, 4};
	const image_descriptor descriptor(
		make_span(extents),
		2,
		numerical_type::float32
	);
	const image_metadata metadata;
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto file = std::make_shared<mock_image_reader>();

	ALLOW_CALL(*file, get_descriptor()).LR_RETURN(std::ref(descriptor));
	ALLOW_CALL(*file, get_metadata()).LR_RETURN(std::ref(metadata));

	const scratch_image_reader reader(entry, file);

	CHECK( &reader.get_descriptor() == &descriptor );
	CHECK( &reader.get_metadata() == &metadata );
}

TEST_CASE(
	"a scratch_image_reader reads from its entry alone what the entry holds",
	"[scratch_image_reader]"
)
{
	const auto regions = make_plan({5, 6, 7}, {0, 1, 2});
	const auto nothing = make_plan({}, {});
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto file = std::make_shared<mock_image_reader>();
	auto destination = make_test_array();

	REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH( same_regions(_2, regions) )
		.LR_RETURN(nothing);
	FORBID_CALL(*entry, store(trompeloeil::_, trompeloeil::_));
	FORBID_CALL(*file, read(trompeloeil::_, trompeloeil::_));

	const scratch_image_reader reader(entry, file);
	reader.read(array_ref(destination), regions);
}

TEST_CASE(
	"a scratch_image_reader offers its entry the regions it does not hold",
	"[scratch_image_reader]"
)
{
	const auto regions = make_plan({5, 6, 7}, {0, 1, 2});
	const auto missing = make_plan({6, 7}, {1, 2});
	const auto nothing = make_plan({}, {});
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto file = std::make_shared<mock_image_reader>();
	auto destination = make_test_array();
	trompeloeil::sequence order;

	REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH( same_regions(_2, regions) )
		.IN_SEQUENCE(order)
		.LR_RETURN(missing);
	REQUIRE_CALL(*entry, store(trompeloeil::_, trompeloeil::_))
		.LR_WITH( &_1 == file.get() && same_regions(_2, missing) )
		.IN_SEQUENCE(order);

	SECTION( "what the entry took in is read from it" )
	{
		REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
			.LR_WITH( same_regions(_2, missing) )
			.IN_SEQUENCE(order)
			.LR_RETURN(nothing);
		FORBID_CALL(*file, read(trompeloeil::_, trompeloeil::_));

		const scratch_image_reader reader(entry, file);
		reader.read(array_ref(destination), regions);
	}

	SECTION( "what it did not take in is read from the file" )
	{
		const auto unheld = make_plan({7}, {2});

		REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
			.LR_WITH( same_regions(_2, missing) )
			.IN_SEQUENCE(order)
			.LR_RETURN(unheld);
		REQUIRE_CALL(*file, read(trompeloeil::_, trompeloeil::_))
			.LR_WITH( same_regions(_2, unheld) )
			.IN_SEQUENCE(order);

		const scratch_image_reader reader(entry, file);
		reader.read(array_ref(destination), regions);
	}
}

TEST_CASE(
	"a scratch_image_reader reports what reading the file reported",
	"[scratch_image_reader]"
)
{
	const auto regions = make_plan({5, 6}, {0, 1});
	const auto missing = make_plan({6}, {1});
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto file = std::make_shared<mock_image_reader>();
	auto destination = make_test_array();
	trompeloeil::sequence order;

	REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
		.IN_SEQUENCE(order)
		.LR_RETURN(missing);

	SECTION( "when the entry can not take the regions in" )
	{
		REQUIRE_CALL(*entry, store(trompeloeil::_, trompeloeil::_))
			.IN_SEQUENCE(order)
			.SIDE_EFFECT( throw std::runtime_error("from the file") );
		FORBID_CALL(*file, read(trompeloeil::_, trompeloeil::_));

		const scratch_image_reader reader(entry, file);

		REQUIRE_THROWS_AS(
			reader.read(array_ref(destination), regions),
			std::runtime_error
		);
	}

	SECTION( "when what the entry does not hold can not be read" )
	{
		REQUIRE_CALL(*entry, store(trompeloeil::_, trompeloeil::_))
			.IN_SEQUENCE(order);
		REQUIRE_CALL(*entry, read(trompeloeil::_, trompeloeil::_))
			.IN_SEQUENCE(order)
			.LR_RETURN(missing);
		REQUIRE_CALL(*file, read(trompeloeil::_, trompeloeil::_))
			.IN_SEQUENCE(order)
			.SIDE_EFFECT( throw std::runtime_error("from the file") );

		const scratch_image_reader reader(entry, file);

		REQUIRE_THROWS_AS(
			reader.read(array_ref(destination), regions),
			std::runtime_error
		);
	}
}
