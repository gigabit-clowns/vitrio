// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_metadata_fields.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i) + 0.25F;
	}

	return values;
}

} // anonymous namespace

TEST_CASE(
	"a TIFF file created through the selectors reads back as it was written",
	"[tiff][image_file_format_selector]"
)
{
	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	image_file_read_format_selector readers;
	readers.register_builtin_formats();

	// The two shapes a TIFF file can hold, under both of its extensions.
	const std::vector<std::vector<std::size_t>> shapes = {
		{3, 4},
		{2, 3, 4}
	};
	const std::vector<std::string> names = {
		"round_trip_selectors.tif",
		"round_trip_selectors.tiff"
	};

	for (const auto &name : names)
	{
		for (const auto &extents : shapes)
		{
			const scoped_path path(name);
			const auto values = counting(count_elements(extents));

			auto source =
				make_host_array<float>(extents, numerical_type::float32);
			std::memcpy(
				source.get_data(),
				values.data(),
				values.size() * sizeof(float)
			);

			const image_descriptor descriptor(
				make_span(extents),
				2,
				numerical_type::float32
			);

			{
				const auto writer =
					writers.open(
						path.get(),
						descriptor,
						image_metadata(),
						*test::make_file_transfer_pool()
					);
				write_now(*writer, source, whole_of(extents));
				flush_now(*writer);
			}

			const auto reader =
		readers.open(path.get(), *make_file_transfer_pool());

			REQUIRE( reader->get_descriptor() == descriptor );

			auto destination =
				make_host_array<float>(extents, numerical_type::float32);
			read_now(*reader, destination, whole_of(extents));

			REQUIRE( get_values<float>(destination) ==
				values );
		}
	}
}

TEST_CASE(
	"a TIFF file converts to and from the type it holds",
	"[tiff][image_file_format_selector]"
)
{
	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	image_file_read_format_selector readers;
	readers.register_builtin_formats();

	const std::vector<std::size_t> extents = {2, 2};
	const std::vector<float> values = {0.0F, 1.0F, 100.0F, 127.0F};

	const numerical_type file_types[] = {
		numerical_type::int8,
		numerical_type::uint8,
		numerical_type::int16,
		numerical_type::uint16,
		numerical_type::int32,
		numerical_type::uint32,
		numerical_type::int64,
		numerical_type::uint64,
		numerical_type::float16,
		numerical_type::float32,
		numerical_type::float64
	};

	for (const auto file_type : file_types)
	{
		const scoped_path path("round_trip_conversion.tif");

		auto source = make_host_array<float>(extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		{
			const auto writer = writers.open(
				path.get(),
				image_descriptor(make_span(extents), 2, file_type),
				image_metadata(),
				*test::make_file_transfer_pool()
			);
			write_now(*writer, source, whole_of(extents));
			flush_now(*writer);
		}

		const auto reader =
		readers.open(path.get(), *make_file_transfer_pool());

		REQUIRE( reader->get_descriptor().get_data_type() == file_type );

		// Read back into a wider type than the file holds, which is the
		// conversion a caller asks for rather than the one the file forces.
		auto destination =
			make_host_array<double>(extents, numerical_type::float64);
		read_now(*reader, destination, whole_of(extents));

		REQUIRE( get_values<double>(destination) ==
			std::vector<double>(values.begin(), values.end()) );
	}
}

TEST_CASE(
	"a TIFF stack is written a page at a time and read back whole",
	"[tiff][image_file_format_selector]"
)
{
	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	image_file_read_format_selector readers;
	readers.register_builtin_formats();

	const scoped_path path("round_trip_paged.tif");
	const std::vector<std::size_t> extents = {4, 3, 5};
	const std::vector<std::size_t> page = {3, 5};
	const auto values = counting(count_elements(extents));

	auto source = make_host_array<float>(extents, numerical_type::float32);
	std::memcpy(
		source.get_data(),
		values.data(),
		values.size() * sizeof(float)
	);

	{
		const auto writer = writers.open(
			path.get(),
			image_descriptor(make_span(extents), 2, numerical_type::int16),
			image_metadata(),
			*test::make_file_transfer_pool()
		);

		for (std::size_t index = 0; index < extents[0]; ++index)
		{
			image_transfer_plan regions(image_transfer_shape(page, 3, 3));
			regions.add(
				make_span(std::vector<std::size_t>{index, 0, 0}),
				make_span(std::vector<std::size_t>{index, 0, 0})
			);
			write_now(*writer, source, regions);
		}
	}

	const auto reader =
		readers.open(path.get(), *make_file_transfer_pool());
	auto destination =
		make_host_array<std::int16_t>(extents, numerical_type::int16);
	read_now(*reader, destination, whole_of(extents));

	// The file holds integers, so the quarter every value carries is gone.
	std::vector<std::int16_t> expected(values.size());
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		expected[i] = static_cast<std::int16_t>(i);
	}

	REQUIRE( get_values<std::int16_t>(destination) ==
		expected );
}

TEST_CASE(
	"a TIFF file is written and read by the whole array functions",
	"[tiff][image_write][image_read]"
)
{
	const auto writers =
		image_file_write_format_selector::get_shared();
	const auto readers = std::make_shared<file_image_reader_provider>(
		image_file_read_format_selector::get_shared(),
		make_file_transfer_pool()
	);

	SECTION( "a stack" )
	{
		const scoped_path path("round_trip_whole_stack.tif");
		const std::vector<std::size_t> extents = {3, 4, 5};
		const auto values = counting(count_elements(extents));

		auto source = make_host_array<float>(extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		write_stack(
			source,
			path.get(),
			*writers,
			*test::make_file_transfer_pool()
		);

		const auto read = vitrio::read(path.get(), *readers);

		REQUIRE( read.get_descriptor() ==
			make_contiguous_array_descriptor(
				make_span(extents), numerical_type::float32) );
		REQUIRE( get_values<float>(read) == values );
	}

	SECTION( "a single image" )
	{
		const scoped_path path("round_trip_whole_single.tif");
		const std::vector<std::size_t> extents = {4, 5};
		const auto values = counting(count_elements(extents));

		auto source = make_host_array<float>(extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		write_single(
			source,
			path.get(),
			*writers,
			*test::make_file_transfer_pool()
		);

		const auto read = vitrio::read(path.get(), *readers);

		REQUIRE( get_values<float>(read) == values );
	}
}

TEST_CASE(
	"what a TIFF file can not hold is refused through the selectors",
	"[tiff][image_file_format_selector]"
)
{
	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	const scoped_path path("round_trip_refused.tif");

	SECTION( "a stack of one image" )
	{
		const std::vector<std::size_t> extents = {1, 3, 4};

		REQUIRE_THROWS_AS(
			writers.open(
				path.get(),
				image_descriptor(
					make_span(extents), 2, numerical_type::float32),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a volume" )
	{
		const std::vector<std::size_t> extents = {2, 3, 4};

		REQUIRE_THROWS_AS(
			writers.open(
				path.get(),
				image_descriptor(
					make_span(extents), 3, numerical_type::float32),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "part of a page" )
	{
		const std::vector<std::size_t> extents = {3, 4};
		const std::vector<std::size_t> half = {3, 2};

		auto source = make_host_array<float>(half, numerical_type::float32);
		const auto writer = writers.open(
			path.get(),
			image_descriptor(make_span(extents), 2, numerical_type::float32),
			image_metadata(),
			*test::make_file_transfer_pool()
		);

		image_transfer_plan regions(image_transfer_shape(half, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{0, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		REQUIRE_THROWS_AS(
			write_now(*writer, source, regions),
			unsupported_operation_error
		);
	}
}

TEST_CASE(
	"the metadata a TIFF file is created with is what it states",
	"[tiff][image_file_format_selector]"
)
{
	using namespace image_metadata_fields;
	using Catch::Matchers::WithinRel;

	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	image_file_read_format_selector readers;
	readers.register_builtin_formats();

	const scoped_path path("round_trip_metadata.tif");

	// A file states what its first page does, so nothing is stated until
	// that page is written.
	const auto create = [&writers, &path] (
		const std::vector<std::size_t> &extents,
		const image_metadata &metadata
	)
	{
		const auto values = counting(count_elements(extents));
		auto source =
			make_host_array<float>(extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		const auto writer = writers.open(
			path.get(),
			image_descriptor(make_span(extents), 2, numerical_type::float32),
			metadata,
			*test::make_file_transfer_pool()
		);
		write_now(*writer, source, whole_of(extents));
		flush_now(*writer);
	};

	// A resolution is held as a quotient of two integers of 32 bits, so a
	// pixel size comes back as near as that holds it.
	const auto tolerance = 1.0e-6;

	SECTION( "the pixel sizes of an image" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, 0.83);
		set_field(metadata, pixel_size_y, 0.84);
		create({3, 4}, metadata);

		const auto read = readers.read_metadata(path.get());

		REQUIRE_THAT(
			get_field(read, pixel_size_x), WithinRel(0.83, tolerance) );
		REQUIRE_THAT(
			get_field(read, pixel_size_y), WithinRel(0.84, tolerance) );
		REQUIRE( read.get_entries().size() == 2 );
	}

	SECTION( "the pixel sizes and the texts of a stack" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, 1.06);
		set_field(metadata, pixel_size_y, 1.06);
		metadata.set("tiff.image_description", "a stack of two");
		metadata.set("tiff.software", "something");
		metadata.set("tiff.date_time", "2026:11:26 12:00:00");
		create({2, 3, 4}, metadata);

		const auto read = readers.read_metadata(path.get());

		REQUIRE_THAT(
			get_field(read, pixel_size_x), WithinRel(1.06, tolerance) );
		REQUIRE_THAT(
			get_field(read, pixel_size_y), WithinRel(1.06, tolerance) );
		REQUIRE(
			read.get("tiff.image_description").get_text() ==
			"a stack of two"
		);
		REQUIRE( read.get("tiff.software").get_text() == "something" );
		REQUIRE(
			read.get("tiff.date_time").get_text() == "2026:11:26 12:00:00" );
	}

	SECTION( "a file created with none states nothing" )
	{
		create({3, 4}, image_metadata());

		REQUIRE( readers.read_metadata(path.get()).get_entries().empty() );
	}

	SECTION( "a file written again with what it states states the same" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, 0.83);
		set_field(metadata, pixel_size_y, 0.84);
		metadata.set("tiff.software", "something");
		create({3, 4}, metadata);
		const auto first = readers.read_metadata(path.get());

		create({3, 4}, first);

		const auto second = readers.read_metadata(path.get());
		REQUIRE_THAT(
			get_field(second, pixel_size_x),
			WithinRel(get_field(first, pixel_size_x), tolerance)
		);
		REQUIRE_THAT(
			get_field(second, pixel_size_y),
			WithinRel(get_field(first, pixel_size_y), tolerance)
		);
		REQUIRE( second.get("tiff.software") == first.get("tiff.software") );
	}
}

TEST_CASE(
	"what an MRC file states goes into a TIFF file as far as it holds it",
	"[tiff][mrc][image_file_format_selector]"
)
{
	using namespace image_metadata_fields;
	using Catch::Matchers::WithinRel;

	image_file_write_format_selector writers;
	writers.register_builtin_formats();
	image_file_read_format_selector readers;
	readers.register_builtin_formats();

	const scoped_path mrc_path("round_trip_metadata_source.mrc");
	const scoped_path tiff_path("round_trip_metadata_converted.tif");
	const std::vector<std::size_t> extents = {3, 4};
	const image_descriptor descriptor(
		make_span(extents), 2, numerical_type::float32);

	auto source = make_host_array<float>(extents, numerical_type::float32);
	const auto values = counting(count_elements(extents));
	std::memcpy(
		source.get_data(), values.data(), values.size() * sizeof(float));

	image_metadata metadata;
	set_field(metadata, pixel_size_x, 0.83);
	set_field(metadata, pixel_size_y, 0.83);
	{
		const auto writer = writers.open(
			mrc_path.get(), descriptor, metadata,
			*test::make_file_transfer_pool());
		write_now(*writer, source, whole_of(extents));
		flush_now(*writer);
	}

	// The labels of the MRC file are nothing a TIFF file holds, and are
	// left out without anything being refused.
	const auto stated = readers.read_metadata(mrc_path.get());
	REQUIRE( stated.find("mrc.labels") != nullptr );
	{
		const auto writer = writers.open(
			tiff_path.get(), descriptor, stated,
			*test::make_file_transfer_pool());
		write_now(*writer, source, whole_of(extents));
		flush_now(*writer);
	}

	const auto converted = readers.read_metadata(tiff_path.get());

	REQUIRE_THAT(
		get_field(converted, pixel_size_x), WithinRel(0.83, 1.0e-6) );
	REQUIRE_THAT(
		get_field(converted, pixel_size_y), WithinRel(0.83, 1.0e-6) );
	REQUIRE( converted.get_entries().size() == 2 );
}
