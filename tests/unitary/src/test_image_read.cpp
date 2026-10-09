// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_read.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/clipping_image_transfer_sanitizer.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/synchronous_executor.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/index_table.hpp>
#include <vitrio/strict_image_transfer_sanitizer.hpp>

#include "mock/mock_image_loader.hpp"
#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_reader_provider.hpp"
#include "mock/mock_image_scratch.hpp"
#include "mock/mock_image_scratch_entry.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <trompeloeil.hpp>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

std::vector<std::size_t> to_vector(span<const std::size_t> values)
{
	return std::vector<std::size_t>(values.begin(), values.end());
}

std::vector<std::size_t> extents_of(const array &arr)
{
	return to_vector(arr.get_descriptor().get_extents());
}

array make_float_array(const std::vector<std::size_t> &extents)
{
	return make_array(
		make_contiguous_array_descriptor(
			make_span(extents),
			numerical_type::float32
		)
	);
}

index_table make_centres(
	const std::vector<std::vector<std::size_t>> &centres,
	std::size_t rank
)
{
	index_table result(rank);
	for (const auto &centre : centres)
	{
		result.add(make_span(centre));
	}

	return result;
}

// Stacks of eight images of 4 by 4.
const std::vector<std::size_t> stack_extents = {8, 4, 4};
const std::vector<std::size_t> image_extents = {4, 4};

const image_descriptor stack_descriptor(
	make_span(stack_extents),
	2,
	numerical_type::float32
);

using index_list = std::vector<std::size_t>;

// Whether a plan is one region that spans a whole stack.
bool spans_whole_stack(const image_transfer_plan &plan)
{
	const std::vector<std::size_t> origin(stack_extents.size(), 0);

	return
		plan.get_region_count() == 1 &&
		to_vector(plan.get_shape().get_extents()) == stack_extents &&
		to_vector(plan.get_file_offset(0)) == origin;
}

// Whether a plan is one whole image for each of some indices of a stack.
bool spans_images(const image_transfer_plan &plan, const index_list &indices)
{
	if (to_vector(plan.get_shape().get_extents()) != image_extents)
	{
		return false;
	}

	index_list planned;
	for (std::size_t region = 0; region < plan.get_region_count(); ++region)
	{
		const auto offset = to_vector(plan.get_file_offset(region));
		if (offset[1] != 0 || offset[2] != 0)
		{
			return false;
		}

		planned.push_back(offset.front());
	}

	return planned == indices;
}

std::shared_ptr<completion> prefetch(
	image_scratch &scratch,
	std::shared_ptr<image_reader_provider> files,
	const std::vector<image_location> &locations
)
{
	synchronous_executor executor;

	return prefetch_scratch_async(
		scratch,
		std::move(files),
		executor,
		image_location_grouping(make_span(locations))
	);
}

} // anonymous namespace

TEST_CASE(
	"read(path, ...) reads a file into an array covering its whole extents",
	"[image_read]"
)
{
	mock_image_reader_provider readers;
	const std::vector<std::size_t> extents = {3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(extents),
		2,
		numerical_type::float32
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));

	REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_2.get_region_count() == 1 &&
			_2.get_shape().get_file_rank() == 2 &&
			_2.get_shape().get_array_rank() == 2 &&
			to_vector(_2.get_shape().get_extents()) == extents &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{0, 0} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0}
		);

	REQUIRE_CALL(readers, acquire("plane.mrc")).RETURN(reader);

	const auto result = read("plane.mrc", readers);

	CHECK( extents_of(result) == extents );
	CHECK( result.get_descriptor().get_data_type() == numerical_type::float32 );
}

TEST_CASE(
	"read(location, ...) reads the whole file when it carries no index",
	"[image_read]"
)
{
	mock_image_reader_provider readers;
	const std::vector<std::size_t> extents = {3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(extents),
		2,
		numerical_type::float32
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));

	REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_2.get_region_count() == 1 &&
			_2.get_shape().get_file_rank() == 2 &&
			_2.get_shape().get_array_rank() == 2 &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{0, 0} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0}
		);

	REQUIRE_CALL(readers, acquire("plane.mrc")).RETURN(reader);

	const auto result =
		read(image_location("plane.mrc"), readers);

	CHECK( extents_of(result) == extents );
}

TEST_CASE(
	"read(location, ...) reads one slice of a stack into its core shape",
	"[image_read]"
)
{
	// A stack of 4 planes of 3x5: the slowest axis is the stack axis and
	// the trailing two are one plane's core shape.
	mock_image_reader_provider readers;
	const std::vector<std::size_t> file_extents = {4, 3, 5};
	const std::vector<std::size_t> core_extents = {3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(file_extents),
		2,
		numerical_type::int16
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));

	REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_2.get_region_count() == 1 &&
			_2.get_shape().get_file_rank() == 3 &&
			_2.get_shape().get_array_rank() == 2 &&
			to_vector(_2.get_shape().get_extents()) == core_extents &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{2, 0, 0} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0}
		);

	REQUIRE_CALL(readers, acquire("stack.mrcs")).RETURN(reader);

	const auto result =
		read(image_location("stack.mrcs", 2), readers);

	CHECK( extents_of(result) == core_extents );
	CHECK( result.get_descriptor().get_data_type() == numerical_type::int16 );
}

TEST_CASE(
	"read(destination, ...) fills an array the caller has",
	"[image_read]"
)
{
	mock_image_reader_provider readers;
	const std::vector<std::size_t> file_extents = {4, 3, 5};
	const std::vector<std::size_t> core_extents = {3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(file_extents),
		2,
		numerical_type::int16
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));

	SECTION( "the whole file, when the location carries no index" )
	{
		auto destination = make_float_array(file_extents);

		REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
			.LR_WITH(
				_1.get_data() == destination.get_data() &&
				_2.get_region_count() == 1 &&
				to_vector(_2.get_shape().get_extents()) == file_extents &&
				to_vector(_2.get_file_offset(0)) ==
					std::vector<std::size_t>{0, 0, 0}
			);
		REQUIRE_CALL(readers, acquire("stack.mrcs")).RETURN(reader);

		read(array_ref(destination), image_location("stack.mrcs"), readers);
	}

	SECTION( "one image of a stack, when it carries one" )
	{
		auto destination = make_float_array(core_extents);

		REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
			.LR_WITH(
				_1.get_data() == destination.get_data() &&
				_2.get_region_count() == 1 &&
				to_vector(_2.get_shape().get_extents()) == core_extents &&
				to_vector(_2.get_file_offset(0)) ==
					std::vector<std::size_t>{2, 0, 0}
			);
		REQUIRE_CALL(readers, acquire("stack.mrcs")).RETURN(reader);

		read(
			array_ref(destination),
			image_location("stack.mrcs", 2),
			readers
		);
	}
}

TEST_CASE(
	"read(destination, ...) refuses an array of other extents than it reads",
	"[image_read]"
)
{
	mock_image_reader_provider readers;
	const std::vector<std::size_t> file_extents = {4, 3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(file_extents),
		2,
		numerical_type::float32
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));
	ALLOW_CALL(readers, acquire("stack.mrcs")).RETURN(reader);
	FORBID_CALL(*reader, read(trompeloeil::_, trompeloeil::_));

	SECTION( "one that is larger" )
	{
		auto destination = make_float_array({4, 3, 6});

		REQUIRE_THROWS_AS(
			read(
				array_ref(destination),
				image_location("stack.mrcs"),
				readers
			),
			std::invalid_argument
		);
	}

	SECTION( "one of another rank" )
	{
		auto destination = make_float_array({4, 3, 5});

		REQUIRE_THROWS_AS(
			read(
				array_ref(destination),
				image_location("stack.mrcs", 2),
				readers
			),
			std::invalid_argument
		);
	}

	SECTION( "one that is not initialized" )
	{
		REQUIRE_THROWS_AS(
			read(array_ref(), image_location("stack.mrcs"), readers),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"read allocates the data type it is asked for rather than the file's",
	"[image_read]"
)
{
	mock_image_reader_provider readers;
	const std::vector<std::size_t> file_extents = {4, 3, 5};
	const auto reader = std::make_shared<mock_image_reader>();

	const image_descriptor descriptor(
		make_span(file_extents),
		2,
		numerical_type::int16
	);
	ALLOW_CALL(*reader, get_descriptor()).LR_RETURN(std::ref(descriptor));

	REQUIRE_CALL(*reader, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_1.get_descriptor().get_data_type() == numerical_type::float32
		);

	REQUIRE_CALL(readers, acquire("stack.mrcs")).RETURN(reader);

	SECTION( "a file read whole by its path" )
	{
		const auto result =
			read("stack.mrcs", readers, numerical_type::float32);

		CHECK( extents_of(result) == file_extents );
		CHECK(
			result.get_descriptor().get_data_type() ==
			numerical_type::float32
		);
	}

	SECTION( "one image of a stack by its location" )
	{
		const auto result = read(
			image_location("stack.mrcs", 2),
			readers,
			numerical_type::float32
		);

		CHECK( extents_of(result) == std::vector<std::size_t>{3, 5} );
		CHECK(
			result.get_descriptor().get_data_type() ==
			numerical_type::float32
		);
	}
}

TEST_CASE(
	"read_batch_async validates the destination array",
	"[image_read]"
)
{
	// No expectations set on `loader`: none of these calls may reach it.
	mock_image_loader loader;

	SECTION( "a destination with no extents" )
	{
		const std::vector<image_location> locations;

		REQUIRE_THROWS_AS(
			read_batch_async(
				loader,
				make_float_array({}),
				make_span(locations)
			),
			std::invalid_argument
		);
	}

	SECTION( "a batch size that does not match the location count" )
	{
		const std::vector<image_location> locations = {
			image_location("a.mrc"),
			image_location("b.mrc")
		};

		REQUIRE_THROWS_AS(
			read_batch_async(
				loader,
				make_float_array({3, 4, 4}),
				make_span(locations)
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"read_batch_async refuses to mix indexed and unindexed locations",
	"[image_read]"
)
{
	// No expectations set on `loader`: a rejected batch must not reach it.
	mock_image_loader loader;

	SECTION( "an unindexed location following an indexed one" )
	{
		const std::vector<image_location> locations = {
			image_location("stack.mrcs", 0),
			image_location("plain.mrc")
		};

		REQUIRE_THROWS_AS(
			read_batch_async(
				loader,
				make_float_array({2, 4, 4}),
				make_span(locations)
			),
			std::invalid_argument
		);
	}

	SECTION( "an indexed location following an unindexed one" )
	{
		const std::vector<image_location> locations = {
			image_location("plain.mrc"),
			image_location("stack.mrcs", 0)
		};

		REQUIRE_THROWS_AS(
			read_batch_async(
				loader,
				make_float_array({2, 4, 4}),
				make_span(locations)
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"read_batch_async hands an empty batch over as an empty plan",
	"[image_read]"
)
{
	mock_image_loader loader;
	const auto done = std::make_shared<counting_completion>(0);

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( _2.get_region_count() == 0 )
		.RETURN(done);

	const std::vector<image_location> locations;
	const auto completion = read_batch_async(
		loader,
		make_float_array({0, 4, 4}),
		make_span(locations)
	);

	CHECK( completion == done );
}

TEST_CASE(
	"read_batch_async has its regions read as they are stated",
	"[image_read]"
)
{
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( _3 == strict_image_transfer_sanitizer::get_shared() )
		.RETURN(std::make_shared<counting_completion>(0));

	const std::vector<image_location> locations = {
		image_location("a.mrcs", 0)
	};
	read_batch_async(loader, make_float_array({1, 4, 4}), make_span(locations));
}

TEST_CASE(
	"read_batch_async addresses the whole file for unindexed locations",
	"[image_read]"
)
{
	// No index in a stack: every location names a file read as a whole
	// image, so the file rank is the core rank and every file offset stays
	// at the origin.
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			extents_of(_1) == std::vector<std::size_t>{2, 3, 5} &&
			_2.get_region_count() == 2 &&
			_2.get_shape().get_file_rank() == 2 &&
			_2.get_shape().get_array_rank() == 3 &&
			to_vector(_2.get_shape().get_extents()) ==
				std::vector<std::size_t>{3, 5} &&
			_2.get_file(_2.get_region_file(0)) == "a.mrc" &&
			_2.get_file(_2.get_region_file(1)) == "b.mrc" &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{0, 0} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0, 0} &&
			to_vector(_2.get_file_offset(1)) ==
				std::vector<std::size_t>{0, 0} &&
			to_vector(_2.get_array_offset(1)) ==
				std::vector<std::size_t>{1, 0, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const std::vector<image_location> locations = {
		image_location("a.mrc"),
		image_location("b.mrc")
	};

	read_batch_async(loader, make_float_array({2, 3, 5}), make_span(locations));
}

TEST_CASE(
	"read_batch_async uses the index in the stack as the file offset",
	"[image_read]"
)
{
	// Every location carries an index in a stack, so the file rank grows to
	// the array rank and that index becomes the leading file offset, while
	// the leading array offset is the slot.
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			_2.get_region_count() == 3 &&
			_2.get_shape().get_file_rank() == 3 &&
			_2.get_shape().get_array_rank() == 3 &&
			to_vector(_2.get_shape().get_extents()) ==
				std::vector<std::size_t>{4, 4} &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{2, 0, 0} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0, 0} &&
			to_vector(_2.get_file_offset(1)) ==
				std::vector<std::size_t>{0, 0, 0} &&
			to_vector(_2.get_array_offset(1)) ==
				std::vector<std::size_t>{1, 0, 0} &&
			to_vector(_2.get_file_offset(2)) ==
				std::vector<std::size_t>{5, 0, 0} &&
			to_vector(_2.get_array_offset(2)) ==
				std::vector<std::size_t>{2, 0, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const std::vector<image_location> locations = {
		image_location("stack.mrcs", 2),
		image_location("stack.mrcs", 0),
		image_location("stack.mrcs", 5)
	};

	read_batch_async(loader, make_float_array({3, 4, 4}), make_span(locations));
}

TEST_CASE(
	"read_batch_async returns the completion of the loader",
	"[image_read]"
)
{
	mock_image_loader loader;
	const auto pending = std::make_shared<counting_completion>(1);

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.RETURN(pending);

	const std::vector<image_location> locations = { image_location("a.mrc") };
	const auto completion = read_batch_async(
		loader,
		make_float_array({1, 3, 5}),
		make_span(locations)
	);

	CHECK( completion == pending );
}

TEST_CASE(
	"read_patches_async validates the destination against the centres",
	"[image_read]"
)
{
	// No expectations set on `loader`: none of these calls may reach it.
	mock_image_loader loader;
	const image_location location("a.mrc");

	SECTION( "a destination with no extents" )
	{
		const auto centres = make_centres({}, 2);

		REQUIRE_THROWS_AS(
			read_patches_async(loader, make_float_array({}), location, centres),
			std::invalid_argument
		);
	}

	SECTION( "a batch size that does not match the centre count" )
	{
		const auto centres = make_centres({{10, 10}, {20, 20}}, 2);

		REQUIRE_THROWS_AS(
			read_patches_async(
				loader,
				make_float_array({3, 10, 10}),
				location,
				centres
			),
			std::invalid_argument
		);
	}

	SECTION( "centres that do not have the rank of one patch" )
	{
		const auto centres = make_centres({{10, 10, 10}}, 3);

		REQUIRE_THROWS_AS(
			read_patches_async(
				loader,
				make_float_array({1, 10, 10}),
				location,
				centres
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"read_patches_async hands an empty batch over as an empty plan",
	"[image_read]"
)
{
	mock_image_loader loader;
	const auto done = std::make_shared<counting_completion>(0);

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( _2.get_region_count() == 0 )
		.RETURN(done);

	const auto centres = make_centres({}, 2);
	const auto completion = read_patches_async(
		loader,
		make_float_array({0, 10, 10}),
		image_location("a.mrc"),
		centres
	);

	CHECK( completion == done );
}

TEST_CASE(
	"read_patches_async places each patch around its centre",
	"[image_read]"
)
{
	// The corner of a patch is its centre less half its extent, so a patch
	// of ten centred at fifty starts at forty-five, and one of nine centred
	// there starts at forty-six.
	mock_image_loader loader;
	const auto centres = make_centres({{50, 50}}, 2);

	SECTION( "an even extent" )
	{
		REQUIRE_CALL(
			loader,
			load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH(
				to_vector(_2.get_shape().get_extents()) ==
					std::vector<std::size_t>{10, 10} &&
				to_vector(_2.get_file_offset(0)) ==
					std::vector<std::size_t>{45, 45}
			)
			.RETURN(std::make_shared<counting_completion>(0));

		read_patches_async(
			loader,
			make_float_array({1, 10, 10}),
			image_location("a.mrc"),
			centres
		);
	}

	SECTION( "an odd extent" )
	{
		REQUIRE_CALL(
			loader,
			load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH(
				to_vector(_2.get_shape().get_extents()) ==
					std::vector<std::size_t>{9, 9} &&
				to_vector(_2.get_file_offset(0)) ==
					std::vector<std::size_t>{46, 46}
			)
			.RETURN(std::make_shared<counting_completion>(0));

		read_patches_async(
			loader,
			make_float_array({1, 9, 9}),
			image_location("a.mrc"),
			centres
		);
	}
}

TEST_CASE(
	"read_patches_async has its patches clipped at the borders",
	"[image_read]"
)
{
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH( _3 == clipping_image_transfer_sanitizer::get_shared() )
		.RETURN(std::make_shared<counting_completion>(0));

	read_patches_async(
		loader,
		make_float_array({1, 10, 10}),
		image_location("a.mrc"),
		make_centres({{50, 50}}, 2)
	);
}

TEST_CASE(
	"read_patches_async gives each patch its own slot of the batch",
	"[image_read]"
)
{
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			_2.get_file_count() == 1 &&
			_2.get_file(0) == "a.mrc" &&
			_2.get_region_count() == 3 &&
			_2.get_shape().get_file_rank() == 2 &&
			_2.get_shape().get_array_rank() == 3 &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{15, 25} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0, 0} &&
			to_vector(_2.get_file_offset(1)) ==
				std::vector<std::size_t>{35, 45} &&
			to_vector(_2.get_array_offset(1)) ==
				std::vector<std::size_t>{1, 0, 0} &&
			to_vector(_2.get_file_offset(2)) ==
				std::vector<std::size_t>{55, 65} &&
			to_vector(_2.get_array_offset(2)) ==
				std::vector<std::size_t>{2, 0, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const auto centres =
		make_centres({{20, 30}, {40, 50}, {60, 70}}, 2);

	read_patches_async(
		loader,
		make_float_array({3, 10, 10}),
		image_location("a.mrc"),
		centres
	);
}

TEST_CASE(
	"read_patches_async carries the part of a patch before the image in its "
	"array offset",
	"[image_read]"
)
{
	// A patch of ten centred at three starts two rows before the image
	// begins. The region still names a whole patch, starting two rows into
	// its slot and at the first row of the image.
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			_2.get_region_count() == 1 &&
			to_vector(_2.get_shape().get_extents()) ==
				std::vector<std::size_t>{10, 10} &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{0, 45} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 2, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const auto centres = make_centres({{3, 50}}, 2);

	read_patches_async(
		loader,
		make_float_array({1, 10, 10}),
		image_location("a.mrc"),
		centres
	);
}

TEST_CASE(
	"read_patches_async cuts the patches of a stack out of one slice",
	"[image_read]"
)
{
	// A location carrying an index in a stack grows the file rank by the
	// axis the stack is indexed along, which every patch of the batch shares
	// since they all come from one image.
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			_2.get_region_count() == 2 &&
			_2.get_shape().get_file_rank() == 3 &&
			_2.get_shape().get_array_rank() == 3 &&
			to_vector(_2.get_shape().get_extents()) ==
				std::vector<std::size_t>{10, 10} &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{4, 15, 25} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0, 0} &&
			to_vector(_2.get_file_offset(1)) ==
				std::vector<std::size_t>{4, 35, 45} &&
			to_vector(_2.get_array_offset(1)) ==
				std::vector<std::size_t>{1, 0, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const auto centres = make_centres({{20, 30}, {40, 50}}, 2);

	read_patches_async(
		loader,
		make_float_array({2, 10, 10}),
		image_location("stack.mrcs", 4),
		centres
	);
}

TEST_CASE(
	"read_patches_async cuts boxes out of a volume the same way",
	"[image_read]"
)
{
	// Nothing about the function is two dimensional: a subtomogram is a
	// patch of one more axis.
	mock_image_loader loader;

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_WITH(
			_2.get_region_count() == 1 &&
			_2.get_shape().get_file_rank() == 3 &&
			_2.get_shape().get_array_rank() == 4 &&
			to_vector(_2.get_shape().get_extents()) ==
				std::vector<std::size_t>{8, 8, 8} &&
			to_vector(_2.get_file_offset(0)) ==
				std::vector<std::size_t>{16, 26, 36} &&
			to_vector(_2.get_array_offset(0)) ==
				std::vector<std::size_t>{0, 0, 0, 0}
		)
		.RETURN(std::make_shared<counting_completion>(0));

	const auto centres = make_centres({{20, 30, 40}}, 3);

	read_patches_async(
		loader,
		make_float_array({1, 8, 8, 8}),
		image_location("tomogram.mrc"),
		centres
	);
}

TEST_CASE(
	"read_patches_async returns the completion of the loader",
	"[image_read]"
)
{
	mock_image_loader loader;
	const auto pending = std::make_shared<counting_completion>(1);

	REQUIRE_CALL(
		loader,
		load(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.RETURN(pending);

	const auto centres = make_centres({{50, 50}}, 2);
	const auto completion = read_patches_async(
		loader,
		make_float_array({1, 10, 10}),
		image_location("a.mrc"),
		centres
	);

	CHECK( completion == pending );
}

TEST_CASE(
	"prefetch_scratch_async needs a reader provider",
	"[image_read]"
)
{
	mock_image_scratch scratch;

	REQUIRE_THROWS_AS(
		prefetch(scratch, nullptr, {image_location("stack.mrcs", 0)}),
		std::invalid_argument
	);
}

TEST_CASE(
	"prefetch_scratch_async stores what the locations name of each file",
	"[image_read]"
)
{
	mock_image_scratch scratch;
	const auto first_entry = std::make_shared<mock_image_scratch_entry>();
	const auto second_entry = std::make_shared<mock_image_scratch_entry>();
	const auto files = std::make_shared<mock_image_reader_provider>();
	const auto first_file = std::make_shared<mock_image_reader>();
	const auto second_file = std::make_shared<mock_image_reader>();
	trompeloeil::sequence order;

	REQUIRE_CALL(scratch, find("stack_0.mrcs")).RETURN(first_entry);
	REQUIRE_CALL(scratch, find("stack_1.mrcs")).RETURN(second_entry);
	ALLOW_CALL(*first_file, get_descriptor())
		.RETURN(std::ref(stack_descriptor));
	ALLOW_CALL(*second_file, get_descriptor())
		.RETURN(std::ref(stack_descriptor));

	// The files are taken in ascending order of path, each opened once and
	// stored into its own entry.
	REQUIRE_CALL(*files, acquire("stack_0.mrcs"))
		.IN_SEQUENCE(order)
		.RETURN(first_file);
	REQUIRE_CALL(*first_entry, store(trompeloeil::_, trompeloeil::_))
		.LR_WITH( &_1 == first_file.get() && spans_images(_2, {1, 4}) )
		.IN_SEQUENCE(order);
	REQUIRE_CALL(*files, acquire("stack_1.mrcs"))
		.IN_SEQUENCE(order)
		.RETURN(second_file);
	REQUIRE_CALL(*second_entry, store(trompeloeil::_, trompeloeil::_))
		.LR_WITH( &_1 == second_file.get() && spans_images(_2, {2}) )
		.IN_SEQUENCE(order);

	const auto completion = prefetch(
		scratch,
		files,
		{
			image_location("stack_1.mrcs", 2),
			image_location("stack_0.mrcs", 4),
			image_location("stack_0.mrcs", 1)
		}
	);

	REQUIRE( completion != nullptr );
	CHECK( completion->is_ready() );
	CHECK_NOTHROW( completion->get() );
}

TEST_CASE(
	"prefetch_scratch_async stores the whole of a file named as a whole",
	"[image_read]"
)
{
	mock_image_scratch scratch;
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto files = std::make_shared<mock_image_reader_provider>();
	const auto file = std::make_shared<mock_image_reader>();

	REQUIRE_CALL(scratch, find("stack.mrcs")).RETURN(entry);
	ALLOW_CALL(*file, get_descriptor()).RETURN(std::ref(stack_descriptor));
	REQUIRE_CALL(*files, acquire("stack.mrcs")).RETURN(file);
	REQUIRE_CALL(*entry, store(trompeloeil::_, trompeloeil::_))
		.LR_WITH( &_1 == file.get() && spans_whole_stack(_2) );

	const auto completion = prefetch(
		scratch,
		files,
		{image_location("stack.mrcs", 3), image_location("stack.mrcs")}
	);

	CHECK_NOTHROW( completion->get() );
}

TEST_CASE(
	"prefetch_scratch_async skips a file the scratch has no entry for",
	"[image_read]"
)
{
	mock_image_scratch scratch;
	const auto entry = std::make_shared<mock_image_scratch_entry>();
	const auto files = std::make_shared<mock_image_reader_provider>();
	const auto file = std::make_shared<mock_image_reader>();

	REQUIRE_CALL(scratch, find("unheld.mrcs")).RETURN(nullptr);
	REQUIRE_CALL(scratch, find("stack.mrcs")).RETURN(entry);
	ALLOW_CALL(*file, get_descriptor()).RETURN(std::ref(stack_descriptor));

	// The file with no entry is not even opened.
	FORBID_CALL(*files, acquire("unheld.mrcs"));
	REQUIRE_CALL(*files, acquire("stack.mrcs")).RETURN(file);
	REQUIRE_CALL(*entry, store(trompeloeil::_, trompeloeil::_));

	const auto completion = prefetch(
		scratch,
		files,
		{image_location("unheld.mrcs", 0), image_location("stack.mrcs", 0)}
	);

	CHECK( completion->is_ready() );
	CHECK_NOTHROW( completion->get() );
}

TEST_CASE(
	"prefetch_scratch_async reports the first failure once every file is "
	"done",
	"[image_read]"
)
{
	mock_image_scratch scratch;
	const auto first_entry = std::make_shared<mock_image_scratch_entry>();
	const auto second_entry = std::make_shared<mock_image_scratch_entry>();
	const auto files = std::make_shared<mock_image_reader_provider>();
	const auto second_file = std::make_shared<mock_image_reader>();

	REQUIRE_CALL(scratch, find("absent.mrcs")).RETURN(first_entry);
	REQUIRE_CALL(scratch, find("stack.mrcs")).RETURN(second_entry);
	ALLOW_CALL(*second_file, get_descriptor())
		.RETURN(std::ref(stack_descriptor));

	REQUIRE_CALL(*files, acquire("absent.mrcs"))
		.SIDE_EFFECT( throw std::out_of_range("from the provider") )
		.RETURN(nullptr);
	FORBID_CALL(*first_entry, store(trompeloeil::_, trompeloeil::_));

	// The file after the one that failed is still loaded.
	REQUIRE_CALL(*files, acquire("stack.mrcs")).RETURN(second_file);
	REQUIRE_CALL(*second_entry, store(trompeloeil::_, trompeloeil::_));

	const auto completion = prefetch(
		scratch,
		files,
		{image_location("absent.mrcs", 0), image_location("stack.mrcs", 0)}
	);

	CHECK( completion->is_ready() );
	CHECK_THROWS_AS( completion->get(), std::out_of_range );
}

TEST_CASE(
	"prefetch_scratch_async of no location is done at once",
	"[image_read]"
)
{
	mock_image_scratch scratch;
	const auto files = std::make_shared<mock_image_reader_provider>();

	const auto completion = prefetch(scratch, files, {});

	REQUIRE( completion != nullptr );
	CHECK( completion->is_ready() );
	CHECK_NOTHROW( completion->get() );
}
