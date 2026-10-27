// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/crc32c.hpp>

#include <cstring>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

std::uint32_t crc_of(const char *text)
{
	std::vector<byte> bytes(text, text + std::strlen(text));
	return compute_crc32c(make_span(bytes.data(), bytes.size()));
}

} // anonymous namespace

TEST_CASE( "compute_crc32c gives the checksum of Castagnoli", "[crc32c]" )
{
	SECTION( "the check value of the polynomial" )
	{
		REQUIRE( crc_of("123456789") == 0xE3069283U );
	}

	SECTION( "nothing" )
	{
		REQUIRE( crc_of("") == 0U );
	}

	SECTION( "thirty two zeros, a vector of RFC 3720" )
	{
		const std::vector<byte> zeros(32, 0);
		REQUIRE( compute_crc32c(make_span(zeros.data(), zeros.size())) ==
			0x8A9136AAU );
	}

	SECTION( "thirty two bytes of all ones, a vector of RFC 3720" )
	{
		const std::vector<byte> ones(32, 0xFF);
		REQUIRE( compute_crc32c(make_span(ones.data(), ones.size())) ==
			0x62A8AB43U );
	}
}
