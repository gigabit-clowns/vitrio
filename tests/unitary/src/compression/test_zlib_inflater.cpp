// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <compression/zlib_inflater.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "compression_test_bytes.hpp"
#include "zlib_test_streams.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

// Inflates a stream handed over in stretches of a given size, into stretches
// of output of another, and tells whether it ended.
bool inflate_in_stretches(
	zlib_inflater &inflater,
	const std::vector<byte> &encoded,
	std::size_t input_stretch,
	std::size_t output_stretch,
	std::vector<byte> &decoded
)
{
	std::size_t given = 0;
	std::size_t produced = 0;
	auto input = make_span(encoded.data(), std::size_t(0));
	bool ended = false;

	for (;;)
	{
		if (input.empty())
		{
			if (given == encoded.size())
			{
				break;
			}
			const auto size =
				std::min(input_stretch, encoded.size() - given);
			input = make_span(encoded.data() + given, size);
			given += size;
		}

		if (produced + output_stretch > decoded.size())
		{
			decoded.resize(produced + output_stretch);
		}
		auto output = make_span(decoded.data() + produced, output_stretch);
		const auto pending = input.size();
		ended = inflater.inflate(input, output);
		produced += output_stretch - output.size();

		// What the inflater leaves after the end of its stream is no more
		// of it.
		if (input.size() == pending && output.size() == output_stretch)
		{
			break;
		}
	}

	decoded.resize(produced);
	return ended;
}

std::vector<byte> concatenate(
	const std::vector<byte> &first,
	const std::vector<byte> &second
)
{
	auto bytes = first;
	bytes.insert(bytes.end(), second.begin(), second.end());
	return bytes;
}

} // anonymous namespace

TEST_CASE( "zlib_inflater inflates a stream however it is handed over",
	"[zlib_inflater]" )
{
	const auto bytes = make_codec_test_bytes(200000);
	const auto encoded = deflate_stream(bytes, 31);
	std::vector<byte> decoded;

	SECTION( "whole, into room for all of it" )
	{
		zlib_inflater inflater(31, false, "test");
		REQUIRE( inflate_in_stretches(
			inflater, encoded, encoded.size(), bytes.size(), decoded) );
		REQUIRE( decoded == bytes );
		REQUIRE( inflater.has_ended() );
	}

	SECTION( "a byte at a time" )
	{
		zlib_inflater inflater(31, false, "test");
		REQUIRE( inflate_in_stretches(inflater, encoded, 1, 4096, decoded) );
		REQUIRE( decoded == bytes );
	}

	SECTION( "into a byte of room at a time" )
	{
		zlib_inflater inflater(31, false, "test");
		REQUIRE( inflate_in_stretches(
			inflater, encoded, encoded.size(), 1, decoded) );
		REQUIRE( decoded == bytes );
	}

	SECTION( "a zlib stream" )
	{
		zlib_inflater inflater(15, false, "test");
		REQUIRE( inflate_in_stretches(
			inflater, deflate_stream(bytes, 15), 777, 333, decoded) );
		REQUIRE( decoded == bytes );
	}
}

TEST_CASE( "zlib_inflater stops where the input or the room runs out",
	"[zlib_inflater]" )
{
	const auto bytes = make_codec_test_bytes(50000);
	const auto encoded = deflate_stream(bytes, 31);

	SECTION( "a stream cut short has not ended" )
	{
		zlib_inflater inflater(31, false, "test");
		auto input = make_span(encoded.data(), encoded.size() - 8);
		std::vector<byte> decoded(bytes.size());
		auto output = make_span(decoded.data(), decoded.size());

		REQUIRE_FALSE( inflater.inflate(input, output) );
		REQUIRE( input.empty() );
		REQUIRE_FALSE( inflater.has_ended() );
	}

	SECTION( "an output that is full leaves the rest of the input" )
	{
		zlib_inflater inflater(31, false, "test");
		auto input = make_span(encoded.data(), encoded.size());
		std::vector<byte> decoded(100);
		auto output = make_span(decoded.data(), decoded.size());

		REQUIRE_FALSE( inflater.inflate(input, output) );
		REQUIRE( output.empty() );
		REQUIRE_FALSE( input.empty() );
		REQUIRE( std::equal(decoded.begin(), decoded.end(), bytes.begin()) );
	}

	SECTION( "nothing is inflated before anything is given" )
	{
		zlib_inflater inflater(31, false, "test");
		REQUIRE_FALSE( inflater.has_ended() );
	}
}

TEST_CASE( "zlib_inflater takes several members only when told to",
	"[zlib_inflater]" )
{
	const auto first = make_codec_test_bytes(30000);
	const auto second = make_codec_test_bytes(20011);
	const auto encoded = concatenate(
		deflate_stream(first, 31),
		deflate_stream(second, 31)
	);
	std::vector<byte> decoded;

	SECTION( "members are inflated one after another" )
	{
		zlib_inflater inflater(31, true, "test");
		REQUIRE( inflate_in_stretches(inflater, encoded, 333, 1000, decoded) );
		REQUIRE( decoded == concatenate(first, second) );
	}

	SECTION( "a member split across inputs is still begun" )
	{
		zlib_inflater inflater(31, true, "test");
		REQUIRE( inflate_in_stretches(inflater, encoded, 1, 1000, decoded) );
		REQUIRE( decoded == concatenate(first, second) );
	}

	SECTION( "without them, what follows the first member is left" )
	{
		zlib_inflater inflater(31, false, "test");
		auto input = make_span(encoded.data(), encoded.size());
		decoded.resize(first.size() + second.size());
		auto output = make_span(decoded.data(), decoded.size());

		REQUIRE( inflater.inflate(input, output) );
		REQUIRE( input.size() == deflate_stream(second, 31).size() );
		REQUIRE( output.size() == second.size() );
	}

	SECTION( "bytes that begin no member are refused" )
	{
		auto garbage = deflate_stream(first, 31);
		garbage.push_back(0);
		garbage.push_back(0);
		zlib_inflater inflater(31, true, "test");

		REQUIRE_THROWS_WITH(
			inflate_in_stretches(inflater, garbage, 1 << 20, 1 << 20, decoded),
			Catch::Matchers::ContainsSubstring("follow the stream")
		);
	}
}

TEST_CASE( "zlib_inflater refuses what is not a stream of its kind",
	"[zlib_inflater]" )
{
	const auto bytes = make_codec_test_bytes(1000);
	std::vector<byte> decoded;

	SECTION( "a zlib stream is not a gzip one" )
	{
		zlib_inflater inflater(31, true, "test");
		REQUIRE_THROWS_AS(
			inflate_in_stretches(
				inflater, deflate_stream(bytes, 15), 100, 100, decoded),
			image_file_format_error
		);
	}

	SECTION( "bytes that are no stream at all" )
	{
		zlib_inflater inflater(31, true, "name");
		REQUIRE_THROWS_WITH(
			inflate_in_stretches(inflater, bytes, 100, 100, decoded),
			Catch::Matchers::StartsWith("name: ") &&
			Catch::Matchers::ContainsSubstring("not one zlib inflates")
		);
	}

	SECTION( "a stream whose checksum does not match" )
	{
		auto encoded = deflate_stream(bytes, 31);
		encoded[encoded.size() - 8] ^= 0xff;
		zlib_inflater inflater(31, false, "test");
		REQUIRE_THROWS_AS(
			inflate_in_stretches(inflater, encoded, 100, 100, decoded),
			image_file_format_error
		);
	}
}
