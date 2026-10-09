// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/tiff/tiff_signature.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::tiff;

namespace
{

bool has_signature_of(const std::vector<std::uint8_t> &values)
{
	std::vector<byte> bytes;
	for (const auto value : values)
	{
		bytes.push_back(byte(value));
	}

	return has_signature(make_span(bytes.data(), bytes.size()));
}

} // anonymous namespace

TEST_CASE( "a file is recognized as TIFF by how it begins",
	"[tiff_signature]" )
{
	SECTION( "a classic file of either byte order is recognized" )
	{
		REQUIRE( has_signature_of({0x49, 0x49, 42, 0}) );
		REQUIRE( has_signature_of({0x4D, 0x4D, 0, 42}) );
	}

	SECTION( "a BigTIFF file of either byte order is recognized" )
	{
		REQUIRE( has_signature_of({0x49, 0x49, 43, 0}) );
		REQUIRE( has_signature_of({0x4D, 0x4D, 0, 43}) );
	}

	SECTION( "what follows the signature does not matter" )
	{
		REQUIRE( has_signature_of({0x49, 0x49, 42, 0, 8, 0, 0, 0}) );
	}

	SECTION( "a version in the other byte order is not recognized" )
	{
		REQUIRE_FALSE( has_signature_of({0x49, 0x49, 0, 42}) );
		REQUIRE_FALSE( has_signature_of({0x4D, 0x4D, 42, 0}) );
	}

	SECTION( "another version is not recognized" )
	{
		REQUIRE_FALSE( has_signature_of({0x49, 0x49, 44, 0}) );
		REQUIRE_FALSE( has_signature_of({0x4D, 0x4D, 0, 41}) );
	}

	SECTION( "two differing byte order marks are not recognized" )
	{
		REQUIRE_FALSE( has_signature_of({0x49, 0x4D, 42, 0}) );
		REQUIRE_FALSE( has_signature_of({0x4D, 0x49, 0, 42}) );
	}

	SECTION( "another byte order mark is not recognized" )
	{
		REQUIRE_FALSE( has_signature_of({0x4A, 0x4A, 42, 0}) );
	}

	SECTION( "too few bytes are not recognized" )
	{
		REQUIRE_FALSE( has_signature_of({}) );
		REQUIRE_FALSE( has_signature_of({0x49, 0x49, 42}) );
	}
}
