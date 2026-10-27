// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_file_write_format.hpp>

#include <formats/zarr/zarr_file_read_format.hpp>
#include <formats/zarr/zarr_json.hpp>

#include <array/fixed_width_float.hpp>

#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include "zarr_test_store.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

template <typename T>
std::vector<T> counting(std::size_t count, T scale = T(1))
{
	std::vector<T> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<T>(static_cast<T>(p) * scale);
	}
	return values;
}

std::shared_ptr<image_writer> create(
	const std::string &path,
	const image_descriptor &descriptor
)
{
	const zarr_file_write_format format;
	return format.open(
		image_file_probe(path), descriptor, image_metadata(),
		*make_file_transfer_pool());
}

std::shared_ptr<image_reader> reopen(const std::string &path)
{
	const zarr_file_read_format format;
	const image_file_probe probe(path);
	REQUIRE( format.get_suitability(probe) ==
		image_file_format_suitability::normal );
	return format.open(probe, *make_file_transfer_pool());
}

template <typename T>
void write_whole(
	image_writer &writer,
	const std::vector<std::size_t> &shape,
	numerical_type type,
	const std::vector<T> &values
)
{
	auto source = make_host_array<T>(shape, type, values);
	write_now(writer, source, whole_of(shape));
	flush_now(writer);
}

template <typename T>
std::vector<T> read_whole(const image_reader &reader, numerical_type type)
{
	const auto values = reader.get_descriptor().get_extents();
	const std::vector<std::size_t> shape(values.begin(), values.end());
	auto destination = make_host_array<T>(shape, type);
	read_now(reader, destination, whole_of(shape));
	return get_values<T>(destination);
}

} // anonymous namespace

TEST_CASE( "zarr_file_write_format claims paths that end in .zarr",
	"[zarr_file_write_format]" )
{
	const zarr_file_write_format format;
	REQUIRE( format.get_name() == "ZARR" );
	REQUIRE( format.get_suitability(image_file_probe("absent.zarr")) ==
		image_file_format_suitability::normal );
	REQUIRE( format.get_suitability(image_file_probe("absent.ZARR/")) ==
		image_file_format_suitability::normal );
	REQUIRE( format.get_suitability(image_file_probe("absent.mrc")) ==
		image_file_format_suitability::unsupported );
	REQUIRE( format.get_suitability(image_file_probe(
		get_zarr_asset_path("ome_v05_tilt_series.zarr/0"))) ==
		image_file_format_suitability::normal );
}

TEST_CASE( "what zarr_file_write_format writes reads back as it was written",
	"[zarr_file_write_format]" )
{
	const scoped_directory directory("test_zarr_write_round_trip");

	SECTION( "a volume, an image of OME-Zarr in a group" )
	{
		const auto path = directory.get() + ".zarr";
		const scoped_directory store("test_zarr_write_round_trip.zarr");
		const extents shape = {7, 9, 300};
		const auto values = counting<float32_t>(7 * 9 * 300, 0.5F);
		{
			const auto writer = create(path, image_descriptor(
				make_span(shape), 3, numerical_type::float32));
			write_whole(*writer, shape, numerical_type::float32, values);
		}

		REQUIRE( boost::filesystem::exists(path + "/zarr.json") );
		REQUIRE( boost::filesystem::exists(path + "/0/zarr.json") );
		const auto reader = reopen(path);
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(shape), 3, numerical_type::float32) );
		REQUIRE( read_whole<float32_t>(*reader, numerical_type::float32) ==
			values );
	}

	SECTION( "a stack of images, which stays a stack" )
	{
		const scoped_directory store("test_zarr_write_stack.zarr");
		const extents shape = {3, 40, 50};
		const auto values = counting<std::uint16_t>(3 * 40 * 50);
		{
			const auto writer = create(store.get(), image_descriptor(
				make_span(shape), 2, numerical_type::uint16));
			write_whole(*writer, shape, numerical_type::uint16, values);
		}

		const auto reader = reopen(store.get());
		REQUIRE( reader->get_descriptor().get_core_rank() == 2 );
		REQUIRE( read_whole<std::uint16_t>(*reader, numerical_type::uint16) ==
			values );
	}

	SECTION( "an array OME-Zarr can not state, plainly" )
	{
		const scoped_directory store("test_zarr_write_line.zarr");
		const extents shape = {2, 2, 2, 2, 2, 3};
		const auto values = counting<std::int32_t>(96, -1);
		{
			const auto writer = create(store.get(), image_descriptor(
				make_span(shape), 1, numerical_type::int32));
			write_whole(*writer, shape, numerical_type::int32, values);
		}

		const auto node = read_json_document(store.get() + "/zarr.json");
		REQUIRE( node["node_type"] == "array" );
		const auto reader = reopen(store.get());
		REQUIRE( reader->get_descriptor().get_core_rank() == 1 );
		REQUIRE( read_whole<std::int32_t>(*reader, numerical_type::int32) ==
			values );
	}

	SECTION( "values converted on their way into the file" )
	{
		const scoped_directory store("test_zarr_write_converted.zarr");
		const extents shape = {5, 6};
		const auto values = counting<double>(30, 2.0);
		{
			const auto writer = create(store.get(), image_descriptor(
				make_span(shape), 2, numerical_type::int16));
			write_whole(*writer, shape, numerical_type::float64, values);
		}

		const auto reader = reopen(store.get());
		REQUIRE( reader->get_descriptor().get_data_type() ==
			numerical_type::int16 );
		REQUIRE( read_whole<double>(*reader, numerical_type::float64) ==
			values );
	}

	SECTION( "what is never written reads as zero" )
	{
		const scoped_directory store("test_zarr_write_sparse.zarr");
		const extents shape = {4, 2000};
		{
			const auto writer = create(store.get(), image_descriptor(
				make_span(shape), 2, numerical_type::uint8));
			image_transfer_plan regions(image_transfer_shape({1, 3}, 2, 2));
			regions.add(make_span(extents{2, 1500}), make_span(extents{0, 0}));
			auto source = make_host_array<std::uint8_t>(
				{1, 3}, numerical_type::uint8, std::vector<std::uint8_t>{
					7, 8, 9});
			write_now(*writer, source, regions);
			flush_now(*writer);
		}

		auto expected = std::vector<std::uint8_t>(8000, 0);
		expected[2 * 2000 + 1500] = 7;
		expected[2 * 2000 + 1501] = 8;
		expected[2 * 2000 + 1502] = 9;
		const auto reader = reopen(store.get());
		REQUIRE( read_whole<std::uint8_t>(*reader, numerical_type::uint8) ==
			expected );
	}
}

TEST_CASE( "zarr_file_write_format replaces stores and files, and nothing else",
	"[zarr_file_write_format]" )
{
	const extents shape = {4, 4};
	const image_descriptor descriptor(
		make_span(shape), 2, numerical_type::uint8);

	SECTION( "a store at the path, with all it holds" )
	{
		const scoped_directory store("test_zarr_write_replace.zarr");
		write_store_file(store.get(), "zarr.json",
			R"({"zarr_format": 3, "node_type": "group"})");
		write_store_file(store.get(), "old/c/0/0", "old");

		create(store.get(), descriptor);
		REQUIRE_FALSE( boost::filesystem::exists(store.get() + "/old") );
		REQUIRE( boost::filesystem::exists(store.get() + "/0/zarr.json") );
	}

	SECTION( "a file at the path" )
	{
		const scoped_directory store("test_zarr_write_replace_file.zarr");
		std::ofstream(store.get().c_str()) << "a file";

		create(store.get(), descriptor);
		REQUIRE( boost::filesystem::is_directory(store.get()) );
	}

	SECTION( "a directory that holds anything else is refused, and kept" )
	{
		const scoped_directory store("test_zarr_write_refuse.zarr");
		write_store_file(store.get(), "notes.txt", "keep me");

		REQUIRE_THROWS_AS(create(store.get(), descriptor), image_file_error);
		REQUIRE( boost::filesystem::exists(store.get() + "/notes.txt") );
	}

	SECTION( "a type ZARR has not is refused before anything is replaced" )
	{
		const scoped_directory store("test_zarr_write_refuse_type.zarr");
		write_store_file(store.get(), "zarr.json",
			R"({"zarr_format": 3, "node_type": "group"})");

		REQUIRE_THROWS_AS(
			create(store.get(), image_descriptor(
				make_span(shape), 2, numerical_type::complex_float16)),
			unsupported_operation_error);
		REQUIRE( boost::filesystem::exists(store.get() + "/zarr.json") );
	}
}
