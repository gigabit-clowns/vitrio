// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/open_raw_image_reader.hpp>

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>
#include <system/asynchronous_file_read_port_opener.hpp>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

const std::vector<std::size_t> stack_extents = {2, 3, 4};

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i);
	}

	return values;
}

// A file of `preamble` bytes followed by `count` float32 values that count
// up from zero, in the byte order of the host.
void write_file(
	const std::string &path,
	std::size_t preamble,
	std::size_t count
)
{
	const auto values = counting(count);
	std::vector<char> raw(preamble + count * sizeof(float), 'p');
	std::memcpy(raw.data() + preamble, values.data(), count * sizeof(float));

	std::ofstream output(path, std::ios::out | std::ios::binary);
	output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

raw_image_layout make_layout(
	std::size_t preamble,
	std::vector<std::ptrdiff_t> strides = {12, 4, 1}
)
{
	return raw_image_layout(
		image_descriptor(
			make_span(stack_extents),
			2,
			numerical_type::float32
		),
		std::move(strides),
		preamble,
		get_system_byte_order()
	);
}

// Every value of the file, read through the reader the factory opens.
std::vector<float>
read_all(const std::string &path, const raw_image_layout &layout)
{
	const auto reader = open_raw_image_reader(
		path,
		layout,
		image_metadata(),
		*make_file_transfer_pool()
	);
	REQUIRE( reader != nullptr );

	auto destination = make_host_array<float>(
		stack_extents,
		numerical_type::float32,
		-1.0F
	);
	read_now(*reader, destination, whole_of(stack_extents));

	return get_values<float>(destination);
}

} // anonymous namespace

TEST_CASE(
	"open_raw_image_reader reads the values wherever they begin",
	"[open_raw_image_reader]"
)
{
	const scoped_path path("open_raw_image_reader_values.raw");

	SECTION( "values aligned for their data type" )
	{
		write_file(path.get(), 16, 24);

		CHECK( read_all(path.get(), make_layout(16)) == counting(24) );
	}

	SECTION( "values that are not" )
	{
		write_file(path.get(), 18, 24);

		CHECK( read_all(path.get(), make_layout(18)) == counting(24) );
	}
}

TEST_CASE(
	"open_raw_image_reader reads unaligned values whose axes are out of "
	"order where it queues reads",
	"[open_raw_image_reader]"
)
{
	if (!asynchronous_file_read_port_opener::is_supported())
	{
		SKIP( "The process can not have asynchronous ports here." );
	}

	const scoped_path path("open_raw_image_reader_out_of_order.raw");
	write_file(path.get(), 18, 24);

	// The element at (i, j, k) is the value at 12 i + j + 3 k.
	std::vector<float> expected;
	for (std::size_t i = 0; i < 2; ++i)
	{
		for (std::size_t j = 0; j < 3; ++j)
		{
			for (std::size_t k = 0; k < 4; ++k)
			{
				expected.push_back(static_cast<float>(12 * i + j + 3 * k));
			}
		}
	}

	CHECK( read_all(path.get(), make_layout(18, {12, 1, 3})) == expected );
}

TEST_CASE(
	"open_raw_image_reader refuses what no reader it opens can read, naming "
	"the file",
	"[open_raw_image_reader]"
)
{
	const scoped_path path("open_raw_image_reader_refused.raw");
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path.get() + ": ")
	);

	SECTION( "a file shorter than its values" )
	{
		write_file(path.get(), 16, 23);

		REQUIRE_THROWS_MATCHES(
			open_raw_image_reader(
				path.get(),
				make_layout(16),
				image_metadata(),
				*make_file_transfer_pool()
			),
			image_file_format_error,
			names_the_file
		);
	}
}

TEST_CASE(
	"open_raw_image_reader refuses unaligned values whose axes are out of "
	"order where it maps the file, naming it",
	"[open_raw_image_reader]"
)
{
	if (asynchronous_file_read_port_opener::is_supported())
	{
		SKIP( "The process queues its reads here, which read any layout." );
	}

	const scoped_path path("open_raw_image_reader_refused_order.raw");
	write_file(path.get(), 18, 24);

	REQUIRE_THROWS_MATCHES(
		open_raw_image_reader(
			path.get(),
			make_layout(18, {12, 1, 3}),
			image_metadata(),
			*make_file_transfer_pool()
		),
		image_file_format_error,
		Catch::Matchers::MessageMatches(
			Catch::Matchers::StartsWith(path.get() + ": ")
		)
	);
}
