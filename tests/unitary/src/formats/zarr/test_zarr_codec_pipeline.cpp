// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_codec_pipeline.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "../../mock/mock_zarr_bytes_codec.hpp"
#include "../../compression/compression_test_bytes.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;
using trompeloeil::_;

TEST_CASE( "zarr_codec_pipeline decodes through its codecs in reverse",
	"[zarr_codec_pipeline]" )
{
	const std::vector<byte> stored = {1, 2, 3, 4, 5, 6};
	const std::vector<byte> middle = {7, 8, 9};
	const std::vector<byte> values = {10, 11, 12, 13};
	std::vector<byte> decoded;

	SECTION( "no codec stores the bytes as they are" )
	{
		const zarr_codec_pipeline pipeline;
		REQUIRE( pipeline.is_empty() );
		REQUIRE( pipeline.get_codec_count() == 0 );

		pipeline.decode(as_span(stored), stored.size(), decoded);
		REQUIRE( decoded == stored );
	}

	SECTION( "the last codec to encode is the first to decode" )
	{
		const auto first = std::make_shared<mock_zarr_bytes_codec>();
		const auto second = std::make_shared<mock_zarr_bytes_codec>();
		const zarr_codec_pipeline pipeline({first, second});
		REQUIRE_FALSE( pipeline.is_empty() );
		REQUIRE( pipeline.get_codec_count() == 2 );

		trompeloeil::sequence order;
		// What it decodes into may be larger than the chunk, before the
		// last step.
		REQUIRE_CALL(*second, decode(_, stored.size(), _))
			.WITH( std::vector<byte>(_1.begin(), _1.end()) == stored )
			.IN_SEQUENCE(order)
			.LR_SIDE_EFFECT( _3 = middle );
		REQUIRE_CALL(*first, decode(_, values.size(), _))
			.WITH( std::vector<byte>(_1.begin(), _1.end()) == middle )
			.IN_SEQUENCE(order)
			.LR_SIDE_EFFECT( _3 = values );

		pipeline.decode(as_span(stored), values.size(), decoded);
		REQUIRE( decoded == values );
	}

	SECTION( "a chunk that decodes into another size is refused" )
	{
		const auto codec = std::make_shared<mock_zarr_bytes_codec>();
		const zarr_codec_pipeline pipeline({codec});

		REQUIRE_CALL(*codec, decode(_, values.size() + 1, _))
			.LR_SIDE_EFFECT( _3 = values );

		REQUIRE_THROWS_AS(
			pipeline.decode(as_span(stored), values.size() + 1, decoded),
			image_file_format_error
		);
	}

	SECTION( "bytes stored as they are must be as many as the chunk holds" )
	{
		const zarr_codec_pipeline pipeline;
		REQUIRE_THROWS_AS(
			pipeline.decode(as_span(stored), stored.size() - 1, decoded),
			image_file_format_error
		);
	}

	SECTION( "a null codec is refused" )
	{
		REQUIRE_THROWS_AS(
			zarr_codec_pipeline({nullptr}),
			std::invalid_argument
		);
	}
}

TEST_CASE( "zarr_codec_pipeline encodes through its codecs in order",
	"[zarr_codec_pipeline]" )
{
	const std::vector<byte> values = {10, 11, 12, 13};
	const std::vector<byte> middle = {7, 8, 9};
	const std::vector<byte> stored = {1, 2};
	std::vector<byte> encoded;

	SECTION( "no codec stores the bytes as they are" )
	{
		zarr_codec_pipeline().encode(as_span(values), encoded);
		REQUIRE( encoded == values );
	}

	SECTION( "the first codec to encode is the first listed" )
	{
		const auto first = std::make_shared<mock_zarr_bytes_codec>();
		const auto second = std::make_shared<mock_zarr_bytes_codec>();
		const zarr_codec_pipeline pipeline({first, second});

		trompeloeil::sequence order;
		REQUIRE_CALL(*first, encode(_, _))
			.WITH( std::vector<byte>(_1.begin(), _1.end()) == values )
			.IN_SEQUENCE(order)
			.LR_SIDE_EFFECT( _2 = middle );
		REQUIRE_CALL(*second, encode(_, _))
			.WITH( std::vector<byte>(_1.begin(), _1.end()) == middle )
			.IN_SEQUENCE(order)
			.LR_SIDE_EFFECT( _2 = stored );

		pipeline.encode(as_span(values), encoded);
		REQUIRE( encoded == stored );
	}
}
