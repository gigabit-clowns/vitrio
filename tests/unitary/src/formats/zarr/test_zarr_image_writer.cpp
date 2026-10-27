// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_image_writer.hpp>

#include "../../fixtures/captured_log.hpp"
#include "../../fixtures/file_transfer_executors.hpp"

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_image_reader.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include "zarr_test_store.hpp"

#include <boost/filesystem/operations.hpp>

#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

const extents shape = {4, 4};

std::shared_ptr<zarr_chunk_store> make_store(const std::string &directory)
{
	write_byte_array_metadata(directory);
	const auto layout = std::make_shared<const zarr_array_layout>(
		read_zarr_node(directory));
	return std::make_shared<zarr_chunk_store>(directory, layout, 1UL << 20);
}

std::unique_ptr<zarr_image_writer> open_writer(
	const std::shared_ptr<zarr_chunk_store> &store,
	std::shared_ptr<file_transfer_executor> transfers
)
{
	return std::make_unique<zarr_image_writer>(
		image_descriptor(make_span(shape), 2, numerical_type::uint8),
		store,
		std::move(transfers)
	);
}

std::vector<std::uint8_t> read_back(
	const zarr_chunk_store &store,
	std::shared_ptr<file_transfer_executor> transfers
)
{
	const zarr_image_reader reader(
		store.get_directory(),
		std::make_shared<const zarr_array_layout>(
			read_zarr_node(store.get_directory())),
		image_descriptor(make_span(shape), 2, numerical_type::uint8),
		image_metadata(),
		std::move(transfers),
		false
	);
	auto destination = make_host_array<std::uint8_t>(
		shape, numerical_type::uint8);
	read_now(reader, destination, whole_of(shape));
	return get_values<std::uint8_t>(destination);
}

std::vector<std::uint8_t> counting()
{
	std::vector<std::uint8_t> values(16);
	for (std::size_t p = 0; p < values.size(); ++p)
	{
		values[p] = static_cast<std::uint8_t>(20 + p);
	}
	return values;
}

// One byte at a position, written from an array of one.
image_transfer_plan one_at(std::size_t row, std::size_t column)
{
	image_transfer_plan regions(image_transfer_shape({1, 1}, 2, 2));
	regions.add(make_span(extents{row, column}), make_span(extents{0, 0}));
	return regions;
}

} // anonymous namespace

TEST_CASE( "a zarr_image_writer writes through every kind of executor",
	"[zarr_image_writer]" )
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_directory directory("test_zarr_image_writer.zarr");
	const auto store = make_store(directory.get());
	const auto writer = open_writer(store, transfers);
	REQUIRE( writer->get_descriptor() == image_descriptor(
		make_span(shape), 2, numerical_type::uint8) );

	const auto values = counting();
	write_now(*writer, make_host_array<std::uint8_t>(
		shape, numerical_type::uint8, values), whole_of(shape));
	flush_now(*writer);
	REQUIRE( read_back(*store, transfers) == values );

	SECTION( "and writes may be under way at once" )
	{
		std::vector<array> sources;
		const auto done = std::make_shared<counting_completion>(16);
		for (std::size_t p = 0; p < 16; ++p)
		{
			sources.push_back(make_host_array<std::uint8_t>(
				{1, 1}, numerical_type::uint8, std::uint8_t(p)));
			writer->write_async(
				sources.back().share_const(), one_at(p / 4, p % 4), done);
		}
		done->get();
		flush_now(*writer);

		std::vector<std::uint8_t> expected(16);
		for (std::size_t p = 0; p < 16; ++p)
		{
			expected[p] = static_cast<std::uint8_t>(p);
		}
		REQUIRE( read_back(*store, transfers) == expected );
	}
}

TEST_CASE( "a zarr_image_writer flushes itself when it is destroyed",
	"[zarr_image_writer]" )
{
	const scoped_directory directory("test_zarr_image_writer_closing.zarr");
	const auto store = make_store(directory.get());
	auto transfers = make_every_file_transfer_executor().front();
	{
		const auto writer = open_writer(store, transfers);
		write_now(*writer, make_host_array<std::uint8_t>(
			{1, 1}, numerical_type::uint8, std::uint8_t(1)), one_at(0, 0));
		REQUIRE( store->get_held_chunks().size() == 1 );
	}

	// The flush goes on after it is gone: destroying the executor waits for
	// what was submitted to it.
	transfers.reset();
	REQUIRE( store->get_held_chunks().empty() );
	REQUIRE( boost::filesystem::exists(
		join_zarr_path(directory.get(), "c/0/0")) );
}

TEST_CASE( "a zarr_image_writer logs a flush that fails as it is destroyed",
	"[zarr_image_writer]" )
{
	const captured_log log;
	const scoped_directory directory("test_zarr_image_writer_failing.zarr");
	const auto store = make_store(directory.get());
	auto transfers = make_every_file_transfer_executor().front();
	{
		const auto writer = open_writer(store, transfers);
		write_now(*writer, make_host_array<std::uint8_t>(
			{1, 1}, numerical_type::uint8, std::uint8_t(1)), one_at(0, 0));

		// The directory of the chunks is a file, so it can not be stored.
		std::ofstream(join_zarr_path(directory.get(), "c").c_str()) << "x";
	}
	transfers.reset();

	REQUIRE_THAT( log.get(),
		Catch::Matchers::ContainsSubstring("while closing it") &&
		Catch::Matchers::ContainsSubstring(directory.get()) );
}

TEST_CASE( "a zarr_image_writer reports what it can not write",
	"[zarr_image_writer]" )
{
	// A chunk that failed to be stored is held, and its flush on closing
	// fails again.
	const captured_log log;
	const scoped_directory directory("test_zarr_image_writer_refusing.zarr");
	const auto store = make_store(directory.get());
	const auto transfers = make_every_file_transfer_executor().front();
	const auto writer = open_writer(store, transfers);
	const auto source = make_host_array<std::uint8_t>(
		shape, numerical_type::uint8);

	SECTION( "a null notifier is refused at once" )
	{
		REQUIRE_THROWS_AS(
			writer->write_async(
				source.share_const(), whole_of(shape), nullptr),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			writer->flush_async(nullptr), std::invalid_argument);
	}

	SECTION( "a region out of the array is reported to the notifier" )
	{
		REQUIRE_THROWS_AS(
			write_now(*writer, make_host_array<std::uint8_t>(
				{1, 1}, numerical_type::uint8), one_at(4, 0)),
			std::out_of_range
		);
	}

	SECTION( "an array that is not initialized is reported" )
	{
		const auto done = std::make_shared<counting_completion>(1);
		writer->write_async(const_array(), whole_of(shape), done);
		REQUIRE_THROWS_AS( done->get(), std::invalid_argument );
	}

	SECTION( "no regions are written at once" )
	{
		const image_transfer_plan none(image_transfer_shape(shape, 2, 2));
		REQUIRE_NOTHROW( write_now(*writer, source, none) );
	}

	SECTION( "a chunk that can not be stored fails the write" )
	{
		std::ofstream(join_zarr_path(directory.get(), "c").c_str()) << "x";
		REQUIRE_THROWS_AS(
			write_now(*writer, source, whole_of(shape)), image_file_error);
	}
}
