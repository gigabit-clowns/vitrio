// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/open_raw_image_reader.hpp>

#include <formats/memory_mapping/image_file_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>

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

image_file_layout make_layout(
	std::size_t preamble,
	std::vector<std::ptrdiff_t> strides = {12, 4, 1}
)
{
	return image_file_layout(
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
read_all(const std::string &path, const image_file_layout &layout)
{
	const auto reader =
		open_raw_image_reader(path, layout, image_metadata());
	REQUIRE( reader != nullptr );

	auto destination = make_host_array<float>(
		stack_extents,
		numerical_type::float32,
		-1.0F
	);
	reader->read(array_ref(destination), whole_of(stack_extents));

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
	"open_raw_image_reader refuses what no reader it opens can read, naming "
	"the file",
	"[open_raw_image_reader]"
)
{
	const scoped_path path("open_raw_image_reader_refused.raw");
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path.get() + ": ")
	);

	SECTION( "unaligned values whose axes are out of order" )
	{
		write_file(path.get(), 18, 24);

		REQUIRE_THROWS_MATCHES(
			open_raw_image_reader(
				path.get(),
				make_layout(18, {12, 1, 3}),
				image_metadata()
			),
			image_file_format_error,
			names_the_file
		);
	}

	SECTION( "a file shorter than its values" )
	{
		write_file(path.get(), 16, 23);

		REQUIRE_THROWS_MATCHES(
			open_raw_image_reader(
				path.get(),
				make_layout(16),
				image_metadata()
			),
			image_file_format_error,
			names_the_file
		);
	}
}
