// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_header.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/span.hpp>

#include "fixtures/dm_test_file.hpp"

#include <cstddef>
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

} // anonymous namespace

TEST_CASE( "a DigitalMicrograph header is recognized by its fields",
	"[dm_header]" )
{
	SECTION( "version 3 in either byte order" )
	{
		REQUIRE( has_dm_signature(
			bytes_of(dm_test_file(3, true, 0).finish())) );
		REQUIRE( has_dm_signature(
			bytes_of(dm_test_file(3, false, 0).finish())) );
	}

	SECTION( "version 4 in either byte order" )
	{
		REQUIRE( has_dm_signature(
			bytes_of(dm_test_file(4, true, 0).finish())) );
		REQUIRE( has_dm_signature(
			bytes_of(dm_test_file(4, false, 0).finish())) );
	}

	SECTION( "other versions are not" )
	{
		auto raw = dm_test_file(3, true, 0).finish();
		raw[3] = 2;
		REQUIRE_FALSE( has_dm_signature(bytes_of(raw)) );
		raw[3] = 5;
		REQUIRE_FALSE( has_dm_signature(bytes_of(raw)) );
	}

	SECTION( "a byte order the format does not state is not" )
	{
		auto raw = dm_test_file(3, true, 0).finish();
		raw[11] = 2;
		REQUIRE_FALSE( has_dm_signature(bytes_of(raw)) );
	}

	SECTION( "a root group whose flags are not flags is not" )
	{
		auto raw = dm_test_file(4, true, 0).finish();
		raw[16] = 7;
		REQUIRE_FALSE( has_dm_signature(bytes_of(raw)) );
	}

	SECTION( "bytes too few to hold the header and its flags are not" )
	{
		const auto raw = dm_test_file(4, true, 0).finish();
		REQUIRE_FALSE( has_dm_signature(bytes_of(
			std::vector<char>(raw.begin(), raw.begin() + 17))) );
		REQUIRE_FALSE( has_dm_signature(bytes_of(std::vector<char>())) );
	}
}

TEST_CASE( "a DigitalMicrograph header is parsed", "[dm_header]" )
{
	SECTION( "version 3 has 32-bit words and its root after 12 bytes" )
	{
		const auto header =
			parse_header(bytes_of(dm_test_file(3, true, 0).finish()));

		REQUIRE( header.get_version() == 3 );
		REQUIRE( header.get_byte_order() == byte_order::little_endian );
		REQUIRE( header.get_word_size() == 4 );
		REQUIRE( header.get_root_offset() == 12 );
	}

	SECTION( "version 4 has 64-bit words and its root after 16 bytes" )
	{
		const auto header =
			parse_header(bytes_of(dm_test_file(4, false, 0).finish()));

		REQUIRE( header.get_version() == 4 );
		REQUIRE( header.get_byte_order() == byte_order::big_endian );
		REQUIRE( header.get_word_size() == 8 );
		REQUIRE( header.get_root_offset() == 16 );
	}

	SECTION( "a version that is not read is refused" )
	{
		auto raw = dm_test_file(3, true, 0).finish();
		raw[3] = 5;
		REQUIRE_THROWS_AS( parse_header(bytes_of(raw)),
			image_file_format_error );
	}

	SECTION( "a byte order the format does not state is refused" )
	{
		auto raw = dm_test_file(4, true, 0).finish();
		raw[15] = 3;
		REQUIRE_THROWS_AS( parse_header(bytes_of(raw)),
			image_file_format_error );
	}

	SECTION( "a header cut short is refused" )
	{
		const auto raw = dm_test_file(4, true, 0).finish();
		REQUIRE_THROWS_AS(
			parse_header(bytes_of(
				std::vector<char>(raw.begin(), raw.begin() + 12))),
			image_file_format_error
		);
		REQUIRE_THROWS_AS(
			parse_header(bytes_of(std::vector<char>(2, '\0'))),
			image_file_format_error
		);
	}
}
