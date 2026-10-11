// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_header.hpp>
#include <formats/dm/dm_tag_parser.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/span.hpp>

#include "fixtures/dm_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;
using namespace vitrio::test;

namespace
{

span<const byte> bytes_of(const std::vector<char> &raw)
{
	return make_span(reinterpret_cast<const byte*>(raw.data()), raw.size());
}

dm_tag parse(const std::vector<char> &raw)
{
	const auto bytes = bytes_of(raw);
	return parse_tags(bytes, parse_header(bytes));
}

} // anonymous namespace

TEST_CASE( "the tags of a DigitalMicrograph file are parsed into a tree",
	"[dm_tag_parser]" )
{
	const std::uint32_t version = GENERATE(3, 4);
	const bool little_endian = GENERATE(true, false);

	dm_test_file file(version, little_endian, 3);
	file.scalar<std::int16_t>("Short", encoding::int16, -2);
	file.group("Group", 2);
	file.datum("Text", {encoding::string, 3}, {'a', 'b', 'c'});
	file.group("", 1);
	const auto values_offset = file.get_size() +
		1 + 2 + (version == 4 ? 8 : 0) + 4 + 4 * (version == 3 ? 4 : 8);
	file.array<float>("", encoding::float32, {1.0f, 2.0f, 3.0f});
	file.datum(
		"Rectangle",
		{
			encoding::structure, 0, 2,
			0, encoding::uint16,
			0, encoding::float64
		},
		std::vector<char>(10, '\0')
	);

	const auto root = parse(file.finish());

	REQUIRE( root.is_group() );
	REQUIRE( root.get_label().empty() );
	REQUIRE( root.get_child_count() == 3 );

	const auto &scalar = root.get_child(0);
	REQUIRE( scalar.get_label() == "Short" );
	REQUIRE( scalar.get_payload_size() == 2 );

	const auto &group = root.get_child(1);
	REQUIRE( group.is_group() );
	REQUIRE( group.get_label() == "Group" );
	REQUIRE( group.get_child_count() == 2 );
	REQUIRE( group.get_child(0).get_payload_size() == 3 );

	const auto &nested = group.get_child(1);
	REQUIRE( nested.is_group() );
	REQUIRE( nested.get_child_count() == 1 );
	REQUIRE( nested.get_child(0).get_payload_offset() == values_offset );
	REQUIRE( nested.get_child(0).get_payload_size() == 12 );
	REQUIRE( nested.get_child(0).get_encoding().size() == 3 );

	const auto &structure = root.get_child(2);
	REQUIRE( structure.get_label() == "Rectangle" );
	REQUIRE( structure.get_payload_size() == 10 );
}

TEST_CASE( "a damaged tree of tags is refused", "[dm_tag_parser]" )
{
	const std::uint32_t version = GENERATE(3, 4);

	SECTION( "a group that states more entries than the file holds" )
	{
		dm_test_file file(version, true, 2);
		file.scalar<std::int32_t>("Only", encoding::int32, 1);

		REQUIRE_THROWS_AS( parse(file.finish()), image_file_format_error );
	}

	SECTION( "a count far past the end of the file" )
	{
		dm_test_file file(version, true, 0xFFFFFFFF);

		REQUIRE_THROWS_AS( parse(file.finish()), image_file_format_error );
	}

	SECTION( "a value that runs past the end of the file" )
	{
		dm_test_file file(version, true, 1);
		file.datum("Data", {encoding::array, encoding::float32, 1000}, {});

		REQUIRE_THROWS_AS( parse(file.finish()), image_file_format_error );
	}

	SECTION( "an encoding of more words than the file holds" )
	{
		dm_test_file file(version, true, 1);
		file.datum("Data", {}, {});
		auto raw = file.finish();
		// The count of words of the encoding, just written as zero.
		const auto count = raw.size() - 8 - (version == 3 ? 4 : 8);
		raw[count] = '\x7f';

		REQUIRE_THROWS_AS( parse(raw), image_file_format_error );
	}

	SECTION( "an entry of a kind the format does not have" )
	{
		dm_test_file file(version, true, 1);
		file.scalar<std::int32_t>("Kind", encoding::int32, 1);
		auto raw = file.finish();
		raw[version == 3 ? 12 + 2 + 4 : 16 + 2 + 8] = 22;

		REQUIRE_THROWS_AS( parse(raw), image_file_format_error );
	}

	SECTION( "a datum without its delimiter" )
	{
		dm_test_file file(version, true, 1);
		file.scalar<std::int32_t>("A", encoding::int32, 1);
		auto raw = file.finish();
		const auto delimiter = (version == 3 ? 12 + 2 + 4 : 16 + 2 + 8) +
			1 + 2 + 1 + (version == 4 ? 8 : 0);
		REQUIRE( raw[delimiter] == '%' );
		raw[delimiter] = '#';

		REQUIRE_THROWS_AS( parse(raw), image_file_format_error );
	}

	SECTION( "a datum whose encoding is refused" )
	{
		dm_test_file file(version, true, 1);
		file.datum("Bad", {13}, {'x'});

		REQUIRE_THROWS_AS( parse(file.finish()), image_file_format_error );
	}

	SECTION( "groups nested deeper than allowed" )
	{
		// The root is at depth one, so this many groups inside it reach one
		// past the deepest allowed.
		dm_test_file file(version, true, 1);
		for (std::size_t depth = 1; depth < max_group_depth; ++depth)
		{
			file.group("", 1);
		}
		file.group("", 0);

		REQUIRE_THROWS_AS( parse(file.finish()), image_file_format_error );
	}

	SECTION( "groups nested as deep as allowed are not" )
	{
		dm_test_file file(version, true, 1);
		for (std::size_t depth = 2; depth < max_group_depth; ++depth)
		{
			file.group("", 1);
		}
		file.group("", 0);

		REQUIRE_NOTHROW( parse(file.finish()) );
	}

	SECTION( "a file that ends with its header" )
	{
		const auto raw = dm_test_file(version, true, 0).finish();
		const auto header = parse_header(bytes_of(raw));
		const std::vector<char> cut(
			raw.begin(),
			raw.begin() + static_cast<std::ptrdiff_t>(
				header.get_root_offset())
		);

		REQUIRE_THROWS_AS(
			parse_tags(bytes_of(cut), header),
			image_file_format_error
		);
	}
}
