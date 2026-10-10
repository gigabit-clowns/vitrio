// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/eer/eer_strip_decoder.hpp>

#include <formats/eer/eer_encoding.hpp>

#include "fixtures/eer_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using namespace vitrio;
using namespace vitrio::eer;
using namespace vitrio::test;

namespace
{

using positions = std::vector<std::uint32_t>;
using pixels = std::vector<std::pair<std::size_t, eer_test_event>>;

std::pair<std::size_t, eer_test_event> at(
	std::size_t pixel,
	std::uint32_t horizontal,
	std::uint32_t vertical
)
{
	return {pixel, eer_test_event{0, 0, horizontal, vertical}};
}

} // anonymous namespace

TEST_CASE( "a strip decodes into the positions of its events",
	"[eer_strip_decoder]" )
{
	// Strips of 4 columns of pixels, each cut into 4 by 4 subpixels.
	const eer_encoding encoding(7, 2, 2);
	const eer_strip_decoder decoder(encoding, 4);
	positions decoded;

	SECTION( "events land on their pixel and subpixel" )
	{
		const auto stream = encode_eer_strip(
			{at(0, 0, 0), at(1, 3, 1), at(6, 2, 3)}, 8, 7, 2, 2);

		REQUIRE( decoder.decode(make_span(stream), 0, 2, decoded) );
		REQUIRE( decoded == positions{
			0, 0,
			1, 7,
			7, 10
		} );
	}

	SECTION( "the rows of a strip follow those before it" )
	{
		const auto stream = encode_eer_strip(
			{at(5, 1, 2)}, 8, 7, 2, 2);

		REQUIRE( decoder.decode(make_span(stream), 10, 2, decoded) );
		REQUIRE( decoded == positions{(11 << 2) | 2, (1 << 2) | 1} );
	}

	SECTION( "an event on the last pixel ends the strip" )
	{
		const auto stream = encode_eer_strip(
			{at(7, 0, 0)}, 8, 7, 2, 2);

		REQUIRE( decoder.decode(make_span(stream), 0, 2, decoded) );
		REQUIRE( decoded == positions{4, 12} );
	}

	SECTION( "a strip with no events decodes to nothing" )
	{
		const auto stream = encode_eer_strip({}, 8, 7, 2, 2);

		REQUIRE( decoder.decode(make_span(stream), 0, 2, decoded) );
		REQUIRE( decoded.empty() );
	}

	SECTION( "positions are appended to those there" )
	{
		decoded = {9, 9};
		const auto stream = encode_eer_strip({at(0, 0, 0)}, 8, 7, 2, 2);

		REQUIRE( decoder.decode(make_span(stream), 0, 2, decoded) );
		REQUIRE( decoded == positions{9, 9, 0, 0} );
	}
}

TEST_CASE( "a run too long for a code is cut into codes of no event",
	"[eer_strip_decoder]" )
{
	// Runs of 3 bits reach 7 pixels at most, so the gap of 20 before the
	// second event takes two codes of 7 that place nothing, of the run bits
	// alone, and one of 6 that places it.
	const eer_encoding encoding(3, 1, 1);
	const eer_strip_decoder decoder(encoding, 10);
	positions decoded;

	const auto stream = encode_eer_strip(
		{at(1, 1, 0), at(22, 0, 1), at(28, 1, 1)}, 30, 3, 1, 1);

	REQUIRE( decoder.decode(make_span(stream), 0, 3, decoded) );
	REQUIRE( decoded == positions{
		0, 3,
		5, 4,
		5, 17
	} );
}

TEST_CASE( "a subpixel is stored with the bit its width counts flipped",
	"[eer_strip_decoder]" )
{
	// With 2 bits, the stored 2 is the subpixel 0 and the stored 0 is 2.
	const eer_encoding encoding(7, 2, 2);
	const eer_strip_decoder decoder(encoding, 4);
	eer_bit_writer stream;
	stream.write(0 | (2u << 7) | (0u << 9), 11);
	stream.write(3, 11);
	const auto bytes = stream.finish();
	positions decoded;

	REQUIRE( decoder.decode(make_span(bytes), 0, 1, decoded) );
	REQUIRE( decoded == positions{2, 0} );
}

TEST_CASE( "a malformed strip is told", "[eer_strip_decoder]" )
{
	const eer_encoding encoding(7, 2, 2);
	const eer_strip_decoder decoder(encoding, 4);
	positions decoded;

	SECTION( "a run past the last pixel" )
	{
		eer_bit_writer stream;
		stream.write(9, 11);
		const auto bytes = stream.finish();

		REQUIRE_FALSE( decoder.decode(make_span(bytes), 0, 2, decoded) );
	}

	SECTION( "a stream that ends within a code placing an event" )
	{
		const std::vector<byte> bytes = {0};

		REQUIRE_FALSE( decoder.decode(make_span(bytes), 0, 2, decoded) );
	}

	SECTION( "a stream that ends before the last pixel keeps its events" )
	{
		eer_bit_writer stream;
		stream.write(1 | (2u << 7) | (2u << 9), 11);
		const auto written = stream.finish();
		const std::vector<byte> bytes(written.begin(), written.begin() + 2);

		REQUIRE( decoder.decode(make_span(bytes), 0, 2, decoded) );
		REQUIRE( decoded == positions{0, 4} );
	}
}
