// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <formats/zarr/zarr_image_reader.hpp>

#include "../../fixtures/file_transfer_executors.hpp"

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <array/fixed_width_float.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <system/asynchronous_file_transfer_port_opener.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

// The tomogram of the assets, 5 by 6 by 7 floats of 0.25 p - 4, through a
// given executor.
std::unique_ptr<zarr_image_reader> open_tomogram(
	std::shared_ptr<file_transfer_executor> transfers,
	bool asynchronous
)
{
	const auto directory = get_zarr_asset_path("v2_float32_blosc_lz4.zarr");
	const auto layout = std::make_shared<const zarr_array_layout>(
		read_zarr_node(directory));
	return std::make_unique<zarr_image_reader>(
		directory,
		layout,
		image_descriptor(
			make_span(extents{5, 6, 7}), 3, numerical_type::float32),
		image_metadata(),
		std::move(transfers),
		asynchronous
	);
}

std::vector<float64_t> expected_tomogram()
{
	std::vector<float64_t> values(210);
	for (std::size_t p = 0; p < values.size(); ++p)
	{
		values[p] = 0.25 * static_cast<double>(p) - 4;
	}
	return values;
}

} // anonymous namespace

TEST_CASE( "a zarr_image_reader reads through every kind of executor",
	"[zarr_image_reader]" )
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));

	// The blocking executor reads through tasks of their own; the
	// asynchronous one through the ports, where the process has them.
	for (const bool asynchronous : {false, true})
	{
		if (asynchronous &&
			!asynchronous_file_transfer_port_opener::is_supported())
		{
			continue;
		}

		const auto reader = open_tomogram(transfers, asynchronous);
		REQUIRE( reader->get_descriptor().get_core_rank() == 3 );

		auto destination = make_host_array<float64_t>(
			{5, 6, 7}, numerical_type::float64, -1.0);
		read_now(*reader, destination, whole_of({5, 6, 7}));
		REQUIRE( get_values<float64_t>(destination) == expected_tomogram() );
	}
}

TEST_CASE( "reads of a zarr_image_reader may be under way at once",
	"[zarr_image_reader]" )
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const auto reader = open_tomogram(transfers, false);

	std::vector<array> destinations;
	const auto done = std::make_shared<counting_completion>(8);
	for (int i = 0; i < 8; ++i)
	{
		destinations.push_back(make_host_array<float64_t>(
			{5, 6, 7}, numerical_type::float64, -1.0));
		reader->read_async(
			destinations.back().share(), whole_of({5, 6, 7}), done);
	}
	done->get();

	for (const auto &destination : destinations)
	{
		REQUIRE( get_values<float64_t>(destination) == expected_tomogram() );
	}
}

TEST_CASE( "a read of a zarr_image_reader goes on after the reader is gone",
	"[zarr_image_reader]" )
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));

	auto destination = make_host_array<float64_t>(
		{5, 6, 7}, numerical_type::float64, -1.0);
	const auto done = std::make_shared<counting_completion>(1);
	{
		const auto reader = open_tomogram(transfers, false);
		reader->read_async(destination.share(), whole_of({5, 6, 7}), done);
	}

	CHECK_NOTHROW( done->get() );
	CHECK( get_values<float64_t>(destination) == expected_tomogram() );
}

TEST_CASE( "a zarr_image_reader reports what it can not read",
	"[zarr_image_reader]" )
{
	const auto transfers = make_every_file_transfer_executor().front();
	const auto reader = open_tomogram(transfers, false);
	auto destination = make_host_array<float32_t>(
		{5, 6, 7}, numerical_type::float32);

	SECTION( "a null notifier is refused at once" )
	{
		REQUIRE_THROWS_AS(
			reader->read_async(
				destination.share(), whole_of({5, 6, 7}), nullptr),
			std::invalid_argument
		);
	}

	SECTION( "a region out of the array is reported to the notifier" )
	{
		image_transfer_plan regions(image_transfer_shape({5, 6, 7}, 3, 3));
		regions.add(make_span(extents{1, 0, 0}), make_span(extents{0, 0, 0}));

		const auto done = std::make_shared<counting_completion>(1);
		reader->read_async(destination.share(), regions, done);
		REQUIRE_THROWS_AS( done->get(), std::out_of_range );
	}

	SECTION( "an array that is not initialized is reported" )
	{
		const auto done = std::make_shared<counting_completion>(1);
		reader->read_async(array(), whole_of({5, 6, 7}), done);
		REQUIRE_THROWS_AS( done->get(), std::invalid_argument );
	}
}
