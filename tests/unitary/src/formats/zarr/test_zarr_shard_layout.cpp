// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_shard_layout.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

zarr_shard_layout make_layout(
	extents shard,
	const extents &chunk,
	bool checksum = true
)
{
	return zarr_shard_layout(
		std::move(shard), make_span(chunk), zarr_index_location::end,
		byte_order::little_endian, checksum);
}

} // anonymous namespace

TEST_CASE( "zarr_shard_layout places chunks in their shards",
	"[zarr_shard_layout]" )
{
	const auto layout = make_layout({4, 6}, {2, 3});

	SECTION( "a shard holds the chunks that tile it" )
	{
		const auto per_shard = layout.get_chunks_per_shard();
		REQUIRE( extents(per_shard.begin(), per_shard.end()) ==
			extents{2, 2} );
		REQUIRE( layout.get_chunk_count() == 4 );
		REQUIRE( layout.get_index_byte_size() == 4 * 16 + 4 );
		REQUIRE( make_layout({4, 6}, {2, 3}, false).get_index_byte_size() ==
			64 );
	}

	SECTION( "a chunk lies in the shard its position divides into" )
	{
		std::vector<std::size_t> shard;
		const extents chunk = {3, 4};
		layout.get_shard(make_span(chunk), shard);
		REQUIRE( shard == extents{1, 2} );
		REQUIRE( layout.get_index_entry(make_span(chunk)) == 2 * 1 + 0 );
	}

	SECTION( "chunks that do not divide a shard are refused" )
	{
		REQUIRE_THROWS_AS(make_layout({4, 6}, {3, 3}), std::invalid_argument);
		REQUIRE_THROWS_AS(make_layout({4, 6}, {2}), std::invalid_argument);
		REQUIRE_THROWS_AS(make_layout({4, 6}, {0, 3}), std::invalid_argument);
	}

	SECTION( "an index too large to count is refused" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max() / 4;
		REQUIRE_THROWS_AS(
			make_layout({huge, 4}, {1, 1}), std::overflow_error);
	}
}
