// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/mrc/mrc_metadata.hpp>

#include <formats/mrc/mrc_constants.hpp>
#include <formats/mrc/mrc_geometry.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata_fields.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::mrc;
using namespace vitrio::image_metadata_fields;

namespace
{

using cell_type = mrc_header::vector3f_type;

// A volume of 4 columns, 3 rows and 5 sections, whose cell is sampled as
// many times along each axis of space, and whose columns, rows and
// sections run along X, Y and Z.
mrc_header make_volume_header()
{
	mrc_header header;
	header.set_column_count(4);
	header.set_row_count(3);
	header.set_section_count(5);
	header.set_column_sampling(4);
	header.set_row_sampling(3);
	header.set_section_sampling(5);
	header.set_space_group(volume_space_group);
	header.set_column_axis(1);
	header.set_row_axis(2);
	header.set_section_axis(3);
	return header;
}

// A file of images of 4 columns and 3 rows, as many of them as sections.
mrc_header make_image_header(std::int32_t sections = 1)
{
	auto header = make_volume_header();
	header.set_section_count(sections);
	header.set_section_sampling(1);
	header.set_space_group(image_stack_space_group);
	return header;
}

mrc_header with_cell(mrc_header header, const cell_type &cell)
{
	header.set_cell_size(cell);
	return header;
}

mrc_header with_axes(
	mrc_header header,
	std::int32_t column_axis,
	std::int32_t row_axis,
	std::int32_t section_axis
)
{
	header.set_column_axis(column_axis);
	header.set_row_axis(row_axis);
	header.set_section_axis(section_axis);
	return header;
}

image_metadata make_pixel_sizes(double x, double y, double z)
{
	image_metadata metadata;
	set_field(metadata, pixel_size_x, x);
	set_field(metadata, pixel_size_y, y);
	set_field(metadata, pixel_size_z, z);
	return metadata;
}

std::vector<std::string> labels_of(const mrc_header &header)
{
	const auto labels = header.get_labels();
	return std::vector<std::string>(labels.begin(), labels.end());
}

mrc_header applied(const image_metadata &metadata, mrc_header header)
{
	apply_metadata(metadata, mrc_single_section::image, header);
	return header;
}

} // anonymous namespace

TEST_CASE(
	"the pixel sizes of an MRC volume are its cell over its sampling",
	"[mrc_metadata]"
)
{
	SECTION( "along the three axes of space" )
	{
		const auto metadata = derive_metadata(
			with_cell(make_volume_header(), {{2.0F, 3.0F, 7.5F}}));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );
	}

	// The axis correspondence and the sampling of EMD-3001: its columns run
	// along Z, its rows along X and its sections along Y, and the sampling
	// is that of the cell along X, Y and Z, not that of the columns, the
	// rows and the sections.
	SECTION( "whichever of them the columns, rows and sections run along" )
	{
		auto header = with_axes(make_volume_header(), 3, 1, 2);
		header.set_column_count(73);
		header.set_row_count(43);
		header.set_section_count(25);
		header.set_column_sampling(40);
		header.set_row_sampling(12);
		header.set_section_sampling(72);
		header.set_space_group(4);
		header.set_cell_size({{20.0F, 3.0F, 18.0F}});

		const auto metadata = derive_metadata(header);

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 0.25 );
		REQUIRE( get_field(metadata, pixel_size_z) == 0.25 );
	}

	SECTION( "and as though they ran along X, Y and Z where none is named" )
	{
		const auto metadata = derive_metadata(with_axes(
			with_cell(make_volume_header(), {{2.0F, 3.0F, 7.5F}}), 0, 0, 0));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );
	}

	SECTION( "for each volume of a stack of them" )
	{
		auto header = with_cell(make_volume_header(), {{2.0F, 3.0F, 7.5F}});
		header.set_section_count(10);
		header.set_space_group(first_volume_stack_space_group);

		const auto metadata = derive_metadata(header);

		REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );
	}
}

TEST_CASE(
	"the pixel sizes of an MRC image are stated along its two axes only",
	"[mrc_metadata]"
)
{
	const cell_type cell = {{2.0F, 3.0F, 7.5F}};

	SECTION( "a single image" )
	{
		const auto metadata =
			derive_metadata(with_cell(make_image_header(), cell));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "a stack of images, whose third axis is no axis of space" )
	{
		const auto metadata =
			derive_metadata(with_cell(make_image_header(6), cell));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "a stack of one image" )
	{
		const auto metadata = derive_metadata(
			with_cell(make_image_header(), cell),
			mrc_single_section::image_stack
		);

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	// The layout puts the axis along the lower axis of space last, so that
	// is the one x names, whether the columns or the rows run along it.
	SECTION( "an image whose columns run along Y and rows along X" )
	{
		const auto metadata = derive_metadata(
			with_axes(with_cell(make_image_header(), cell), 2, 1, 3));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
	}

	SECTION( "an image whose rows run along Z" )
	{
		auto header = with_axes(with_cell(make_image_header(), cell), 1, 3, 2);
		header.set_section_sampling(5);

		const auto metadata = derive_metadata(header);

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.5 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}
}

TEST_CASE(
	"an MRC header states no pixel size along an axis it has no cell for",
	"[mrc_metadata]"
)
{
	SECTION( "a cell of zeros, as a header that was never given one has" )
	{
		const auto metadata = derive_metadata(make_volume_header());

		REQUIRE( metadata.get_entries().empty() );
	}

	SECTION( "a cell with no size along one axis states the other two" )
	{
		const auto metadata = derive_metadata(
			with_cell(make_volume_header(), {{2.0F, 3.0F, 0.0F}}));

		REQUIRE( get_field(metadata, pixel_size_x) == 0.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "a negative size" )
	{
		const auto metadata = derive_metadata(
			with_cell(make_volume_header(), {{-2.0F, 3.0F, 7.5F}}));

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
	}

	SECTION( "a size that is no number, or no finite one" )
	{
		const auto nan = std::numeric_limits<float>::quiet_NaN();
		const auto infinity = std::numeric_limits<float>::infinity();

		const auto metadata = derive_metadata(
			with_cell(make_volume_header(), {{nan, infinity, 7.5F}}));

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( find_field(metadata, pixel_size_y) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );
	}

	SECTION( "a sampling of zero, or below it" )
	{
		auto header = with_cell(make_volume_header(), {{2.0F, 3.0F, 7.5F}});
		header.set_column_sampling(0);
		header.set_row_sampling(-3);

		const auto metadata = derive_metadata(header);

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( find_field(metadata, pixel_size_y) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );
	}
}

TEST_CASE(
	"the labels of an MRC header are stated one a line",
	"[mrc_metadata]"
)
{
	auto header = make_volume_header();

	SECTION( "a header with none states none" )
	{
		REQUIRE( find_field(derive_metadata(header), labels_field) == nullptr );
	}

	SECTION( "a header with one" )
	{
		header.add_label("Created by something");

		REQUIRE(
			get_field(derive_metadata(header), labels_field) ==
			"Created by something"
		);
	}

	SECTION( "a header with several, in their order" )
	{
		header.add_label("first");
		header.add_label("");
		header.add_label("third");

		REQUIRE(
			get_field(derive_metadata(header), labels_field) ==
			"first\n\nthird"
		);
	}
}

TEST_CASE(
	"metadata is not derived from a header that names its axes wrongly",
	"[mrc_metadata]"
)
{
	auto header = with_axes(make_volume_header(), 1, 1, 3);

	REQUIRE_THROWS_AS( derive_metadata(header), image_file_format_error );
	REQUIRE_THROWS_AS(
		apply_metadata(image_metadata(), mrc_single_section::image, header),
		image_file_format_error
	);
}

TEST_CASE(
	"the pixel sizes given for an MRC file become the size of its cell",
	"[mrc_metadata]"
)
{
	SECTION( "of a volume, along the three axes" )
	{
		const auto header =
			applied(make_pixel_sizes(0.5, 1.0, 1.5), make_volume_header());

		REQUIRE( header.get_cell_size() == cell_type{{2.0F, 3.0F, 7.5F}} );
	}

	SECTION( "of a volume whose columns do not run along X" )
	{
		auto header = with_axes(make_volume_header(), 3, 1, 2);
		header.set_column_sampling(40);
		header.set_row_sampling(12);
		header.set_section_sampling(72);

		apply_metadata(
			make_pixel_sizes(0.5, 0.25, 0.25),
			mrc_single_section::image,
			header
		);

		REQUIRE( header.get_cell_size() == cell_type{{20.0F, 3.0F, 18.0F}} );
	}

	SECTION( "of an image, whose cell has no size along the third axis" )
	{
		const auto header =
			applied(make_pixel_sizes(0.5, 1.0, 1.5), make_image_header(6));

		REQUIRE( header.get_cell_size() == cell_type{{2.0F, 3.0F, 0.0F}} );
	}

	SECTION( "an integer is a pixel size as any other" )
	{
		image_metadata metadata;
		metadata.set("pixel_size_x", 2);

		const auto header = applied(metadata, make_volume_header());

		REQUIRE( header.get_cell_size() == cell_type{{8.0F, 0.0F, 0.0F}} );
	}
}

TEST_CASE(
	"an MRC cell keeps what no pixel size is given for",
	"[mrc_metadata]"
)
{
	SECTION( "metadata that states nothing leaves a header as it was" )
	{
		const auto header = applied(image_metadata(), make_volume_header());

		REQUIRE( header.get_cell_size() == cell_type{{0.0F, 0.0F, 0.0F}} );
		REQUIRE( header.get_labels().empty() );
	}

	SECTION( "one axis given leaves the other two" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_y, 1.0);

		const auto header = applied(metadata, make_volume_header());

		REQUIRE( header.get_cell_size() == cell_type{{0.0F, 3.0F, 0.0F}} );
	}
}

TEST_CASE(
	"a pixel size an MRC header can not hold is left out",
	"[mrc_metadata]"
)
{
	const cell_type untouched = {{0.0F, 0.0F, 0.0F}};

	SECTION( "zero, or below it" )
	{
		const auto header =
			applied(make_pixel_sizes(0.0, -1.0, -0.0), make_volume_header());

		REQUIRE( header.get_cell_size() == untouched );
	}

	SECTION( "no number, or no finite one" )
	{
		const auto header = applied(
			make_pixel_sizes(
				std::numeric_limits<double>::quiet_NaN(),
				std::numeric_limits<double>::infinity(),
				-std::numeric_limits<double>::infinity()
			),
			make_volume_header()
		);

		REQUIRE( header.get_cell_size() == untouched );
	}

	SECTION( "a cell too large or too small for the field it is held in" )
	{
		const auto header = applied(
			make_pixel_sizes(1.0e39, 1.0e-60, 1.0),
			make_volume_header()
		);

		REQUIRE( header.get_cell_size() == cell_type{{0.0F, 0.0F, 5.0F}} );
	}
}

TEST_CASE(
	"the labels given for an MRC file are appended to its header",
	"[mrc_metadata]"
)
{
	auto header = make_volume_header();
	image_metadata metadata;

	SECTION( "one a line, after those the header already has" )
	{
		header.add_label("already there");
		set_field(metadata, labels_field, "first\nsecond");

		apply_metadata(metadata, mrc_single_section::image, header);

		REQUIRE( labels_of(header) ==
			std::vector<std::string>({"already there", "first", "second"}) );
	}

	SECTION( "a line that is no label is left out, and the rest are kept" )
	{
		set_field(
			metadata,
			labels_field,
			"first\n" + std::string(size::label + 1, 'x') + "\na\tb\nlast"
		);

		apply_metadata(metadata, mrc_single_section::image, header);

		REQUIRE( labels_of(header) ==
			std::vector<std::string>({"first", "last"}) );
	}

	SECTION( "a label as long as a label may be is kept" )
	{
		const std::string longest(size::label, 'x');
		set_field(metadata, labels_field, longest);

		apply_metadata(metadata, mrc_single_section::image, header);

		REQUIRE( labels_of(header) == std::vector<std::string>({longest}) );
	}

	SECTION( "the labels past the last the header has room for are dropped" )
	{
		std::string text;
		for (std::size_t i = 0; i < label_count + 3; ++i)
		{
			text += (i > 0 ? "\n" : "") + std::to_string(i);
		}
		set_field(metadata, labels_field, text);

		apply_metadata(metadata, mrc_single_section::image, header);

		const auto labels = labels_of(header);
		REQUIRE( labels.size() == label_count );
		REQUIRE( labels.front() == "0" );
		REQUIRE( labels.back() == std::to_string(label_count - 1) );
	}

	SECTION( "what is stated under the label as anything but a text is "
		"left out" )
	{
		metadata.set("mrc.labels", 3);

		REQUIRE_NOTHROW(
			apply_metadata(metadata, mrc_single_section::image, header) );
		REQUIRE( header.get_labels().empty() );
	}
}

TEST_CASE(
	"what another format states is nothing to an MRC header",
	"[mrc_metadata]"
)
{
	image_metadata metadata;
	metadata.set("tiff.software", "something else");
	metadata.set("application.frames", 40);

	const auto header = applied(metadata, make_volume_header());

	REQUIRE( header.get_cell_size() == cell_type{{0.0F, 0.0F, 0.0F}} );
	REQUIRE( header.get_labels().empty() );
}

TEST_CASE(
	"what an MRC header is given is what it states",
	"[mrc_metadata]"
)
{
	SECTION( "a volume" )
	{
		auto metadata = make_pixel_sizes(0.5, 1.0, 1.5);
		set_field(metadata, labels_field, "first\nsecond");

		REQUIRE(
			derive_metadata(applied(metadata, make_volume_header())) ==
			metadata
		);
	}

	SECTION( "an image, of which z is not stated" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_x, 0.5);
		set_field(metadata, pixel_size_y, 1.0);

		REQUIRE(
			derive_metadata(applied(metadata, make_image_header())) ==
			metadata
		);
	}

	// The header of a file as it is created, through make_header.
	SECTION( "a header built from a descriptor" )
	{
		const std::vector<std::size_t> extents = {5, 3, 4};
		auto header = make_header(
			image_descriptor(make_span(extents), 3, numerical_type::float32));
		const auto metadata = make_pixel_sizes(0.5, 1.0, 1.5);

		apply_metadata(metadata, mrc_single_section::image, header);

		REQUIRE( derive_metadata(header) == metadata );
	}
}
