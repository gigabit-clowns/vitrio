// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_json.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include "zarr_test_store.hpp"

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

TEST_CASE( "read_json_document reads a document or says why not",
	"[zarr_json]" )
{
	const scoped_directory directory("test_zarr_json_documents");

	SECTION( "a document" )
	{
		write_store_file(directory.get(), "a.json", R"({"a": [1, 2]})");
		REQUIRE( read_json_document(directory.get("a.json")) ==
			json::parse(R"({"a": [1, 2]})") );
	}

	SECTION( "what is not JSON, naming the file" )
	{
		write_store_file(directory.get(), "b.json", "{\"a\": ");
		REQUIRE_THROWS_MATCHES(
			read_json_document(directory.get("b.json")),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(directory.get("b.json") + ": "))
		);
	}

	SECTION( "a file that is not there" )
	{
		REQUIRE_THROWS_AS(
			read_json_document(directory.get("absent.json")),
			image_file_error
		);
	}
}

TEST_CASE( "the helpers of zarr_json check what metadata holds",
	"[zarr_json]" )
{
	const auto object = json::parse(
		R"({"shape": [3, 0, 7], "bad": [1, -2], "odd": [1.5], "text": "a"})");

	SECTION( "a member that is there" )
	{
		REQUIRE( get_member(object, "text", "test") == "a" );
	}

	SECTION( "a member that is not, or no object, is malformed" )
	{
		REQUIRE_THROWS_AS(
			get_member(object, "absent", "test"), image_file_format_error);
		REQUIRE_THROWS_AS(
			get_member(json::array(), "a", "test"), image_file_format_error);
	}

	SECTION( "extents are whole numbers of zero or more" )
	{
		REQUIRE( get_extents(object["shape"], "shape", "test") ==
			std::vector<std::size_t>{3, 0, 7} );
		REQUIRE_THROWS_AS(
			get_extents(object["bad"], "shape", "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			get_extents(object["odd"], "shape", "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			get_extents(object["text"], "shape", "test"),
			image_file_format_error);
	}

	SECTION( "the two kinds of refusal" )
	{
		REQUIRE_THROWS_WITH(
			reject_metadata("here", "Why."), "here: Why.");
		REQUIRE_THROWS_AS(
			refuse_metadata("here", "Why."), unsupported_operation_error);
	}
}
