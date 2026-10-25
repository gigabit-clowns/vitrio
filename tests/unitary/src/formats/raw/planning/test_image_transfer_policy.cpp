// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/image_transfer_policy.hpp>

#include <system/page_size.hpp>
#include <tuning.hpp>

#include <cstddef>

using namespace vitrio;

TEST_CASE( "a policy reports what it was constructed with",
	"[image_transfer_policy]" )
{
	const image_transfer_policy policy(64, 4096, 16, 3);

	CHECK( policy.get_gap_tolerance() == 64 );
	CHECK( policy.get_byte_budget() == 4096 );
	CHECK( policy.get_alignment() == 16 );
	CHECK( policy.get_alignment_origin() == 3 );
}

TEST_CASE( "the policy of advice scales its tolerance with the region",
	"[image_transfer_policy]" )
{
	SECTION( "a region wider than the cap is capped" )
	{
		const auto policy =
			make_prefetch_policy(4 * get_prefetch_gap_cap());

		CHECK( policy.get_gap_tolerance() == get_prefetch_gap_cap() );
	}

	SECTION( "a region narrower than the cap sets the tolerance" )
	{
		// Floored at a page, so the region has to be wider than one to be
		// what decides it.
		const std::size_t span = get_prefetch_gap_cap() / 2;
		const auto policy = make_prefetch_policy(span);

		CHECK( policy.get_gap_tolerance() <= get_prefetch_gap_cap() );
		CHECK( policy.get_gap_tolerance() >= span );
	}

	SECTION( "its grid is the pages of the machine, from the file's start" )
	{
		// One plane of 3x4 float32 values.
		const auto policy = make_prefetch_policy(48);

		CHECK( policy.get_alignment() == get_page_size() );
		CHECK( policy.get_alignment_origin() == 0 );
		CHECK( policy.get_byte_budget() == get_prefetch_byte_budget() );
	}
}
