// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_shard_index.hpp>

#include <formats/zarr/crc32c.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <cstdint>
#include <limits>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

const std::uint64_t empty = std::numeric_limits<std::uint64_t>::max();

void put(std::vector<byte> &bytes, std::uint64_t value, bool big)
{
	for (int i = 0; i < 8; ++i)
	{
		const auto shift = big ? 8 * (7 - i) : 8 * i;
		bytes.push_back(static_cast<byte>(value >> shift));
	}
}

std::vector<byte> make_index(
	const std::vector<std::uint64_t> &entries,
	bool big,
	bool checksum
)
{
	std::vector<byte> bytes;
	for (const auto value : entries)
	{
		put(bytes, value, big);
	}
	if (checksum)
	{
		const auto crc = compute_crc32c(make_span(bytes.data(), bytes.size()));
		for (int i = 0; i < 4; ++i)
		{
			bytes.push_back(static_cast<byte>(crc >> (8 * i)));
		}
	}

	return bytes;
}

zarr_shard_layout make_layout(byte_order order, bool checksum)
{
	const std::vector<std::size_t> chunk = {1};
	return zarr_shard_layout(
		{2}, make_span(chunk), zarr_index_location::end, order, checksum);
}

span<const byte> as_span(const std::vector<byte> &bytes)
{
	return make_span(bytes.data(), bytes.size());
}

} // anonymous namespace

TEST_CASE( "zarr_shard_index reads where each chunk of a shard lies",
	"[zarr_shard_index]" )
{
	const auto layout = make_layout(byte_order::little_endian, true);

	SECTION( "a stored chunk and one that is not" )
	{
		const auto bytes = make_index({100, 20, empty, empty}, false, true);
		const zarr_shard_index index(as_span(bytes), layout, 200, "shard");

		const auto first = index.get_entry(0);
		REQUIRE( first.stored );
		REQUIRE( first.offset == 100 );
		REQUIRE( first.size == 20 );
		REQUIRE_FALSE( index.get_entry(1).stored );
	}

	SECTION( "an index of big endian integers, unchecked" )
	{
		const auto bytes = make_index({8, 4, 0, 8}, true, false);
		const zarr_shard_index index(
			as_span(bytes), make_layout(byte_order::big_endian, false), 64,
			"shard");
		REQUIRE( index.get_entry(0).offset == 8 );
		REQUIRE( index.get_entry(1).size == 8 );
	}

	SECTION( "what is not an index of the shard is malformed" )
	{
		auto changed = make_index({100, 20, 0, 4}, false, true);
		changed[0] ^= 1;
		REQUIRE_THROWS_AS(
			zarr_shard_index(as_span(changed), layout, 200, "shard"),
			image_file_format_error);

		const auto shorter = make_index({100, 20}, false, true);
		REQUIRE_THROWS_AS(
			zarr_shard_index(as_span(shorter), layout, 200, "shard"),
			image_file_format_error);

		const auto half = make_index({100, empty, 0, 4}, false, true);
		REQUIRE_THROWS_AS(
			zarr_shard_index(as_span(half), layout, 200, "shard"),
			image_file_format_error);

		const auto beyond = make_index({190, 20, 0, 4}, false, true);
		REQUIRE_THROWS_AS(
			zarr_shard_index(as_span(beyond), layout, 200, "shard"),
			image_file_format_error);

		const auto wrapping = make_index({8, empty - 4, 0, 4}, false, true);
		REQUIRE_THROWS_AS(
			zarr_shard_index(as_span(wrapping), layout, 200, "shard"),
			image_file_format_error);
	}
}
