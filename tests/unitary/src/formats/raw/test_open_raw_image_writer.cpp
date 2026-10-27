// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/open_raw_image_writer.hpp>

#include <formats/raw/queued/queued_image_writer.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_writer.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// What the files below begin with, before their values.
const std::vector<byte> preamble(16, static_cast<byte>('p'));

const std::vector<std::size_t> extents = {2, 3, 4};

raw_image_layout make_layout()
{
	return raw_image_layout(
		image_descriptor(make_span(extents), 2, numerical_type::float32),
		std::vector<std::ptrdiff_t>{12, 4, 1},
		preamble.size(),
		get_system_byte_order()
	);
}

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i) + 0.5F;
	}
	return values;
}

} // anonymous namespace

TEST_CASE(
	"open_raw_image_writer queues the writes through the pool it is given",
	"[open_raw_image_writer]"
)
{
	const scoped_path path("open_raw_image_writer_kind.raw");

	const auto writer = open_raw_image_writer(
		path.get(),
		make_layout(),
		make_span(preamble.data(), preamble.size()),
		*make_file_transfer_pool()
	);

	CHECK( std::dynamic_pointer_cast<queued_image_writer>(writer) );
}

TEST_CASE(
	"open_raw_image_writer writes the preamble and the values where the "
	"layout says",
	"[open_raw_image_writer]"
)
{
	const scoped_path path("open_raw_image_writer_values.raw");
	const auto values = counting(24);

	{
		const auto writer = open_raw_image_writer(
			path.get(),
			make_layout(),
			make_span(preamble.data(), preamble.size()),
			*make_file_transfer_pool()
		);
		const auto source =
			make_host_array<float>(extents, numerical_type::float32, values);
		write_now(*writer, source, whole_of(extents));
		flush_now(*writer);
	}

	std::ifstream input(path.get(), std::ios::binary);
	const std::vector<char> bytes(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);
	REQUIRE(
		bytes.size() == preamble.size() + values.size() * sizeof(float)
	);
	CHECK(
		std::string(bytes.begin(), bytes.begin() + 16) ==
		std::string(16, 'p')
	);

	std::vector<float> stored(values.size());
	std::memcpy(
		stored.data(),
		bytes.data() + preamble.size(),
		stored.size() * sizeof(float)
	);
	CHECK( stored == values );
}
