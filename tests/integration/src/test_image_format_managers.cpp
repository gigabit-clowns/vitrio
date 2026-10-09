// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_read_format_manager.hpp>
#include <vitrio/image_write_format_manager.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>

#include "fixtures/builtin_formats.hpp"

#include <cstddef>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

TEST_CASE( "a file that is not there is claimed by no bundled format",
	"[image_format_manager]" )
{
	const image_probe probe("absent.mrc");

	SECTION( "no format claims a file for reading" )
	{
		// The bundled MRC format reads a file on the identifier its header
		// carries, and a file that is not there carries none. What it does
		// claim is pinned by test_mrc_real_files.cpp.
		const auto manager =
			make_builtin_read_formats();

		REQUIRE( manager->get_most_suitable_format(probe) == nullptr );
		REQUIRE_THROWS_AS(
			manager->open("absent.mrc"),
			image_file_error
		);
	}

	SECTION( "the MRC format claims it for writing" )
	{
		// A file being created does not exist yet, so the extension is the
		// whole of what a write format has to decide on.
		const auto manager =
			make_builtin_write_formats();

		REQUIRE( manager->get_most_suitable_format(probe) != nullptr );
	}

	SECTION( "an extension no format writes is still claimed by none" )
	{
		const auto manager =
			make_builtin_write_formats();
		const image_probe other("absent.eer");
		const std::vector<std::size_t> extents = {2, 2};
		const image_descriptor descriptor(
			make_span(extents),
			2,
			numerical_type::float32
		);

		REQUIRE( manager->get_most_suitable_format(other) == nullptr );
		REQUIRE_THROWS_AS(
			manager->open("absent.eer", descriptor, image_metadata()),
			unsupported_operation_error
		);
	}
}
