// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/tiff/tiff_file_write_format.hpp>

#include <formats/tiff/tiff_file.hpp>
#include <formats/tiff/tiff_image_reader.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_metadata_fields.hpp>
#include <vitrio/image_writer.hpp>

#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>
#include "fixtures/tiff_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::tiff;
using namespace vitrio::test;

namespace
{

image_descriptor describe(
	const std::vector<std::size_t> &extents,
	std::size_t core_rank,
	numerical_type data_type
)
{
	return image_descriptor(make_span(extents), core_rank, data_type);
}

} // anonymous namespace

TEST_CASE( "the TIFF format claims the files it can create",
	"[tiff_file_write_format]" )
{
	const tiff_file_write_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "TIFF" );
	}

	SECTION( "a file of either extension is claimed, existing or not" )
	{
		REQUIRE( format.get_suitability(image_file_probe("absent.tif")) ==
			image_file_format_suitability::normal );
		REQUIRE( format.get_suitability(image_file_probe("absent.tiff")) ==
			image_file_format_suitability::normal );
		REQUIRE( format.get_suitability(image_file_probe("absent.TIF")) ==
			image_file_format_suitability::normal );
	}

	SECTION( "a file of another extension is not" )
	{
		REQUIRE( format.get_suitability(image_file_probe("absent.mrc")) ==
			image_file_format_suitability::unsupported );
		REQUIRE( format.get_suitability(image_file_probe("absent")) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "what an existing file holds does not matter" )
	{
		const scoped_path path("tiff_file_write_format_claimed.tif");
		write_file(path.get(), std::vector<char>(64, 'x'));

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}
}

TEST_CASE( "the TIFF write format creates the file it is asked for",
	"[tiff_file_write_format]" )
{
	const scoped_path path("tiff_file_write_format_created.tif");
	const tiff_file_write_format format;

	SECTION( "an image" )
	{
		const std::vector<std::size_t> extents = {4, 5};
		const auto descriptor = describe(extents, 2, numerical_type::uint8);
		const auto values = counting<std::uint8_t>(20);
		const auto source = make_host_array<std::uint8_t>(
			extents, numerical_type::uint8, values);

		{
			const auto writer = format.open(
				image_file_probe(path.get()), descriptor, image_metadata(),
				*test::make_file_transfer_pool());

			REQUIRE( writer != nullptr );
			REQUIRE( writer->get_descriptor() == descriptor );

			write_now(*writer, source, whole_of(extents));
		}

		const tiff_image_reader reader(path.get());
		auto destination =
			make_host_array<std::uint8_t>(extents, numerical_type::uint8);
		reader.read(array_ref(destination), whole_of(extents));

		REQUIRE( reader.get_descriptor() == descriptor );
		REQUIRE( get_values<std::uint8_t>(destination) == values );
	}

	SECTION( "a stack of images" )
	{
		const std::vector<std::size_t> extents = {3, 4, 5};
		const auto descriptor = describe(extents, 2, numerical_type::float32);
		const auto values = counting<float>(60, 0.5F);
		const auto source = make_host_array<float>(
			extents, numerical_type::float32, values);

		{
			const auto writer = format.open(
				image_file_probe(path.get()), descriptor, image_metadata(),
				*test::make_file_transfer_pool());
			write_now(*writer, source, whole_of(extents));
		}

		const tiff_image_reader reader(path.get());

		REQUIRE( reader.get_descriptor() == descriptor );
	}

	SECTION( "in place of the file that was there" )
	{
		write_file(path.get(), std::vector<char>(64, 'x'));

		const std::vector<std::size_t> extents = {4, 5};
		const auto descriptor = describe(extents, 2, numerical_type::int16);
		const auto source = make_host_array<std::int16_t>(
			extents, numerical_type::int16);

		{
			const auto writer = format.open(
				image_file_probe(path.get()), descriptor, image_metadata(),
				*test::make_file_transfer_pool());
			write_now(*writer, source, whole_of(extents));
		}

		REQUIRE( tiff_image_reader(path.get()).get_descriptor() == descriptor );
	}
}

TEST_CASE( "the TIFF write format refuses what a file can not hold",
	"[tiff_file_write_format]" )
{
	const scoped_path path("tiff_file_write_format_refused.tif");
	const tiff_file_write_format format;
	const image_file_probe probe(path.get());

	SECTION( "a stack of one image, which would read back as an image" )
	{
		const std::vector<std::size_t> extents = {1, 4, 5};

		REQUIRE_THROWS_MATCHES(
			format.open(
				probe,
				describe(extents, 2, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get()) &&
				Catch::Matchers::ContainsSubstring("stack of one")
			)
		);
	}

	SECTION( "a volume" )
	{
		const std::vector<std::size_t> extents = {3, 4, 5};

		REQUIRE_THROWS_AS(
			format.open(
				probe,
				describe(extents, 3, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a single row" )
	{
		const std::vector<std::size_t> extents = {5};

		REQUIRE_THROWS_AS(
			format.open(
				probe,
				describe(extents, 1, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a stack of stacks" )
	{
		const std::vector<std::size_t> extents = {2, 3, 4, 5};

		REQUIRE_THROWS_AS(
			format.open(
				probe,
				describe(extents, 2, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "an extent of zero" )
	{
		const std::vector<std::size_t> extents = {0, 5};

		REQUIRE_THROWS_AS(
			format.open(
				probe,
				describe(extents, 2, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a data type with no sample" )
	{
		const std::vector<std::size_t> extents = {4, 5};

		REQUIRE_THROWS_AS(
			format.open(
				probe,
				describe(extents, 2, numerical_type::complex_float16),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a file that can not be created" )
	{
		const std::vector<std::size_t> extents = {4, 5};
		const auto nowhere =
			get_scratch_path("tiff_file_write_format_no_such_directory") +
			"/a.tif";

		REQUIRE_THROWS_AS(
			format.open(
				image_file_probe(nowhere),
				describe(extents, 2, numerical_type::uint8),
				image_metadata(),
				*test::make_file_transfer_pool()
			),
			image_file_error
		);
	}
}

TEST_CASE( "the TIFF write format writes the metadata it is given",
	"[tiff_file_write_format]" )
{
	using namespace image_metadata_fields;

	const scoped_path path("tiff_file_write_format_metadata.tif");
	const tiff_file_write_format format;
	const std::vector<std::size_t> extents = {4, 5};
	const auto source = make_host_array<std::uint8_t>(
		extents, numerical_type::uint8, counting<std::uint8_t>(20));

	const auto create = [&] (const image_metadata &metadata)
	{
		const auto writer = format.open(
			image_file_probe(path.get()),
			describe(extents, 2, numerical_type::uint8),
			metadata,
			*test::make_file_transfer_pool()
		);
		write_now(*writer, source, whole_of(extents));
	};

	const auto read_tags = [&path]
	{
		tiff_file file(path.get(), tiff_file_mode::read);
		return file.get_page_tags();
	};

	SECTION( "its pixel sizes, as resolutions to a centimetre" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, 1.0);
		set_field(metadata, pixel_size_y, 0.5);
		create(metadata);

		const auto tags = read_tags();

		REQUIRE( tags.get_x_resolution() == 1.0e8 );
		REQUIRE( tags.get_y_resolution() == 2.0e8 );
	}

	SECTION( "its texts" )
	{
		image_metadata metadata;
		metadata.set("tiff.image_description", "a micrograph");
		metadata.set("tiff.software", "something");
		metadata.set("tiff.date_time", "2026:11:26 12:00:00");
		create(metadata);

		const auto tags = read_tags();

		REQUIRE( tags.get_image_description() == "a micrograph" );
		REQUIRE( tags.get_software() == "something" );
		REQUIRE( tags.get_date_time() == "2026:11:26 12:00:00" );
	}

	SECTION( "this library as the software, where none is given" )
	{
		create(image_metadata());

		REQUIRE( read_tags().get_software().compare(0, 7, "vitrio ") == 0 );
	}

	SECTION( "this library as the software, where what is given is none "
		"a page holds" )
	{
		image_metadata metadata;
		metadata.set("tiff.software", 3);
		create(metadata);

		REQUIRE( read_tags().get_software().compare(0, 7, "vitrio ") == 0 );
	}

	SECTION( "and nothing of what a page can not hold" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, -1.0);
		set_field(metadata, pixel_size_z, 1.5);
		metadata.set("tiff.date_time", "yesterday");
		metadata.set("mrc.labels", "Created by something else");

		REQUIRE_NOTHROW( create(metadata) );

		const auto tags = read_tags();

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
		REQUIRE( tags.get_image_description().empty() );
		REQUIRE( tags.get_date_time().empty() );
	}
}
