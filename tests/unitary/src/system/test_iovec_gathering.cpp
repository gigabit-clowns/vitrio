// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/iovec_gathering.hpp>

#include <vitrio/memory_range.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <vector>

using namespace vitrio;

namespace
{

constexpr auto any_size = std::numeric_limits<std::size_t>::max();

} // anonymous namespace

TEST_CASE(
	"gather_iovecs describes what is left of buffers",
	"[iovec_gathering]"
)
{
	std::array<char, 16> bytes{};
	auto *first = bytes.data();
	const std::vector<memory_range> buffers = {
		{first, 4},
		{first + 4, 0},
		{first + 4, 6},
		{first + 10, 6},
	};
	std::array<iovec, 4> vectors{};

	SECTION( "all of them, leaving out those of no bytes" )
	{
		REQUIRE( gather_iovecs(
			make_span(buffers),
			0,
			make_span(vectors),
			any_size
		) == 3 );

		CHECK( vectors[0].iov_base == first );
		CHECK( vectors[0].iov_len == 4 );
		CHECK( vectors[1].iov_base == first + 4 );
		CHECK( vectors[1].iov_len == 6 );
		CHECK( vectors[2].iov_base == first + 10 );
		CHECK( vectors[2].iov_len == 6 );
	}

	SECTION( "past their first bytes" )
	{
		REQUIRE( gather_iovecs(
			make_span(buffers),
			5,
			make_span(vectors),
			any_size
		) == 2 );

		CHECK( vectors[0].iov_base == first + 5 );
		CHECK( vectors[0].iov_len == 5 );
		CHECK( vectors[1].iov_base == first + 10 );
		CHECK( vectors[1].iov_len == 6 );
	}

	SECTION( "as many as there are vectors for" )
	{
		REQUIRE( gather_iovecs(
			make_span(buffers),
			0,
			make_span(vectors.data(), 2),
			any_size
		) == 2 );

		CHECK( vectors[1].iov_base == first + 4 );
	}

	SECTION( "no more bytes than asked for" )
	{
		REQUIRE( gather_iovecs(
			make_span(buffers),
			2,
			make_span(vectors),
			5
		) == 2 );

		CHECK( vectors[0].iov_len == 2 );
		CHECK( vectors[1].iov_len == 3 );
	}

	SECTION( "none where no byte is left" )
	{
		CHECK( gather_iovecs(
			make_span(buffers),
			16,
			make_span(vectors),
			any_size
		) == 0 );
	}
}
