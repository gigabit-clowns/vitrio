// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_chunk_buffers.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

std::vector<byte> filled(std::size_t size, byte value)
{
	return std::vector<byte>(size, value);
}

} // anonymous namespace

TEST_CASE( "zarr_chunk_buffers takes a new buffer of zeros when none is kept",
	"[zarr_chunk_buffers]" )
{
	zarr_chunk_buffers buffers(1024);

	REQUIRE( buffers.get_kept_bytes() == 0 );
	const auto taken = buffers.take(10);
	REQUIRE( taken.size() == 10 );
	REQUIRE( std::count(taken.begin(), taken.end(), 0) == 10 );
	REQUIRE( buffers.take(0).empty() );
}

TEST_CASE( "zarr_chunk_buffers takes a buffer given back, sized without "
	"filling it", "[zarr_chunk_buffers]" )
{
	zarr_chunk_buffers buffers(1024);

	auto given = filled(100, 7);
	given.reserve(200);
	buffers.give(std::move(given));
	REQUIRE( buffers.get_kept_bytes() == 200 );

	const auto taken = buffers.take(150);
	REQUIRE( taken.size() == 150 );
	REQUIRE( taken.capacity() == 200 );
	REQUIRE( taken[99] == 7 );
	REQUIRE( buffers.get_kept_bytes() == 0 );
	REQUIRE( buffers.take(10).capacity() == 10 );
}

TEST_CASE( "zarr_chunk_buffers takes the buffer given back last that fits",
	"[zarr_chunk_buffers]" )
{
	zarr_chunk_buffers buffers(1024);

	buffers.give(filled(30, 1));
	buffers.give(filled(10, 2));
	buffers.give(filled(20, 3));

	// The smaller ones stay.
	auto taken = buffers.take(25);
	REQUIRE( taken.capacity() == 30 );
	REQUIRE( taken[0] == 1 );
	REQUIRE( buffers.get_kept_bytes() == 30 );

	taken = buffers.take(5);
	REQUIRE( taken.capacity() == 20 );
	REQUIRE( taken[0] == 3 );

	// None fits: a new one, and the one kept stays.
	taken = buffers.take(15);
	REQUIRE( taken.capacity() == 15 );
	REQUIRE( buffers.get_kept_bytes() == 10 );
}

TEST_CASE( "zarr_chunk_buffers releases a buffer given back past the budget",
	"[zarr_chunk_buffers]" )
{
	zarr_chunk_buffers buffers(100);

	buffers.give(filled(60, 1));
	buffers.give(filled(60, 2));
	REQUIRE( buffers.get_kept_bytes() == 60 );

	buffers.give(filled(40, 3));
	REQUIRE( buffers.get_kept_bytes() == 100 );

	buffers.give(filled(1, 4));
	REQUIRE( buffers.get_kept_bytes() == 100 );
	REQUIRE( buffers.take(40).capacity() == 40 );
	REQUIRE( buffers.take(1).capacity() == 60 );
}

TEST_CASE( "zarr_chunk_buffers keeps no buffer of no memory",
	"[zarr_chunk_buffers]" )
{
	zarr_chunk_buffers buffers(100);

	buffers.give(std::vector<byte>());
	REQUIRE( buffers.get_kept_bytes() == 0 );
}
