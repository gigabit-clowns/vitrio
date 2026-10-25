// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_chunk_key_encoding.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

namespace
{

std::string key_of(
	const zarr_chunk_key_encoding &encoding,
	const std::vector<std::size_t> &chunk
)
{
	return encoding.get_key(make_span(chunk));
}

} // anonymous namespace

TEST_CASE( "zarr_chunk_key_encoding names the file of each chunk",
	"[zarr_chunk_key_encoding]" )
{
	SECTION( "version 2 joins positions with a dot unless it says" )
	{
		const auto dotted = parse_v2_chunk_key_encoding(
			json::parse(R"({"zarr_format": 2})"), "test");
		REQUIRE( key_of(dotted, {1, 0, 12}) == "1.0.12" );
		REQUIRE( key_of(dotted, {}) == "0" );

		const auto nested = parse_v2_chunk_key_encoding(
			json::parse(R"({"dimension_separator": "/"})"), "test");
		REQUIRE( key_of(nested, {1, 0, 12}) == "1/0/12" );
	}

	SECTION( "version 3 puts c in front by default" )
	{
		const auto encoding = parse_v3_chunk_key_encoding(
			json::parse(R"({"name": "default"})"), "test");
		REQUIRE( key_of(encoding, {3, 4}) == "c/3/4" );
		REQUIRE( key_of(encoding, {}) == "c" );

		const auto dotted = parse_v3_chunk_key_encoding(
			json::parse(
				R"({"name": "default", "configuration": {"separator": "."}})"),
			"test");
		REQUIRE( key_of(dotted, {3, 4}) == "c.3.4" );
	}

	SECTION( "version 3 names chunks as version 2 when told" )
	{
		const auto encoding = parse_v3_chunk_key_encoding(
			json::parse(R"({"name": "v2"})"), "test");
		REQUIRE( key_of(encoding, {3, 4}) == "3.4" );
		REQUIRE( key_of(encoding, {}) == "0" );
	}

	SECTION( "another encoding or separator is malformed" )
	{
		REQUIRE_THROWS_AS(
			parse_v3_chunk_key_encoding(
				json::parse(R"({"name": "other"})"), "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_v2_chunk_key_encoding(
				json::parse(R"({"dimension_separator": "-"})"), "test"),
			image_file_format_error);
	}
}
