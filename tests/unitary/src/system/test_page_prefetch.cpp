// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/page_prefetch.hpp>

#include <vitrio/byte.hpp>

#include <system/page_size.hpp>

#include "../fixtures/aligned_memory.hpp"

#include <vector>

using namespace vitrio;

TEST_CASE(
	"stretches of memory are advised before they are read",
	"[page_prefetch]"
)
{
	const auto page = get_page_size();
	const aligned_memory memory(3 * page, page);
	auto *first = memory.get();

	// Advice leaves nothing a test can observe, so what is asserted is that
	// memory survives being advised what the contract allows: stretches that
	// start on a page and stay within what is mapped.
	SECTION( "a batch of no stretch is advised" )
	{
		const std::vector<memory_range> ranges;

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}

	SECTION( "one whole stretch is advised" )
	{
		const std::vector<memory_range> ranges = {{first, 3 * page}};

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}

	SECTION( "a stretch of less than a page is advised" )
	{
		const std::vector<memory_range> ranges = {{first, 8}};

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}

	SECTION( "a stretch of no bytes is advised" )
	{
		const std::vector<memory_range> ranges = {{first + page, 0}};

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}

	SECTION( "stretches of different memory are advised at once" )
	{
		const aligned_memory other(page, page);
		const std::vector<memory_range> ranges = {
			{first, page},
			{first + 2 * page, page},
			{other.get(), page}
		};

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}

	SECTION( "more stretches than one call takes are advised" )
	{
		const std::vector<memory_range> ranges(200, memory_range(first, page));

		REQUIRE_NOTHROW( prefetch_pages(make_span(ranges)) );
	}
}
