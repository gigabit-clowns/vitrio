// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/page_prefetch.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>

#include <system/page_size.hpp>

#include "../fixtures/mapped_test_file.hpp"

#include <vector>

using namespace vitrio;

TEST_CASE(
	"stretches of a mapped file are advised before they are read",
	"[page_prefetch]"
)
{
	const auto page = get_page_size();
	const mapped_test_file file("page_prefetch_advised.raw", 3 * page);
	const auto bytes = file.get_bytes();
	auto *first = const_cast<vitrio::byte*>(bytes.data());

	const auto advise = [&](const std::vector<memory_range> &ranges)
	{
		prefetch_pages(file.get_file(), bytes, make_span(ranges));
	};

	// Advice leaves nothing a test can observe, so what is asserted is that
	// the file survives being advised what the contract allows: stretches
	// that stay within the mapping, and whose pages do.
	SECTION( "a batch of no stretch is advised" )
	{
		advise({});
	}

	SECTION( "one whole stretch is advised" )
	{
		advise({{first, 3 * page}});
	}

	SECTION( "a stretch of less than a page is advised" )
	{
		advise({{first, 8}});
	}

	SECTION( "a stretch that starts inside a page is advised" )
	{
		advise({{first + 8, page}});
	}

	SECTION( "a stretch of no bytes is advised" )
	{
		advise({{first + page, 0}});
	}

	SECTION( "stretches apart are advised at once" )
	{
		advise({{first, page}, {first + 2 * page, page}});
	}

	SECTION( "more stretches than one call takes are advised" )
	{
		advise(std::vector<memory_range>(200, memory_range(first, page)));
	}

	CHECK( file.holds_its_values() );
}
