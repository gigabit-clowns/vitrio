// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <formats/raw/queued/make_image_write_operation.hpp>

#include "../../../fixtures/file_transfer_executors.hpp"

#include <formats/raw/create_image_file.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

const std::size_t preamble = 16;

std::vector<std::ptrdiff_t>
contiguous_strides(const std::vector<std::size_t> &extents)
{
	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}
	return strides;
}

raw_image_layout make_layout(
	const std::vector<std::size_t> &extents,
	numerical_type data_type,
	std::vector<std::ptrdiff_t> strides,
	byte_order order = get_system_byte_order()
)
{
	return raw_image_layout(
		image_descriptor(make_span(extents), 2, data_type),
		std::move(strides),
		preamble,
		order
	);
}

template <typename T>
std::vector<T> counting(std::size_t first, std::size_t count)
{
	std::vector<T> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<T>(first + i);
	}
	return values;
}

// Every case runs as a process of its own, and several may run at once, so
// the file of a case is named after it.
std::string name_file()
{
	const auto case_name = Catch::getResultCapture().getCurrentTestName();
	const auto tag = std::hash<std::string>()(case_name);

	return "make_image_write_operation_" + std::to_string(tag) + ".raw";
}

// Lays a file out for a layout, filled with a byte, and opens it for the
// writes.
std::shared_ptr<writable_image_file> lay_out(
	const std::string &path,
	const raw_image_layout &layout,
	char fill = 'x'
)
{
	{
		std::ofstream output(path, std::ios::out | std::ios::binary);
		output << std::string(preamble + layout.get_data_size(), fill);
	}
	return std::make_shared<writable_image_file>(path);
}

// The values of a file behind its preamble, as the host reads them.
template <typename T>
std::vector<T> values_of(const std::string &path, std::size_t count)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> bytes(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	std::vector<T> values(count);
	std::memcpy(values.data(), bytes.data() + preamble, count * sizeof(T));
	return values;
}

} // anonymous namespace

TEST_CASE(
	"make_image_write_operation writes regions that lie as they land "
	"straight from the array",
	"[make_image_write_operation]"
)
{
	const auto executor =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {4, 2, 3};
	const auto layout = make_layout(
		extents,
		numerical_type::float32,
		contiguous_strides(extents)
	);

	const auto values = counting<float>(0, 24);
	const auto source =
		make_host_array<float>(extents, numerical_type::float32, values);

	{
		auto file = lay_out(path.get(), layout);
		run_now(
			*executor,
			make_image_write_operation(
				std::move(file),
				layout,
				source.share_const(),
				whole_of(extents),
				1 << 20
			)
		);
	}

	CHECK( values_of<float>(path.get(), 24) == values );
}

TEST_CASE(
	"make_image_write_operation writes regions it converts through memory "
	"of their own, a step at a time",
	"[make_image_write_operation]"
)
{
	const auto executor =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {6, 2, 3};
	const auto layout = make_layout(
		extents,
		numerical_type::float32,
		contiguous_strides(extents)
	);

	const auto values = counting<double>(0, 36);
	const auto source =
		make_host_array<double>(extents, numerical_type::float64, values);

	// One image a step, each region its own, so that the steps take turns
	// at the staging.
	image_transfer_plan regions(image_transfer_shape({2, 3}, 3, 3));
	for (std::size_t image = 0; image < 6; ++image)
	{
		regions.add(
			make_span(std::vector<std::size_t>{image, 0, 0}),
			make_span(std::vector<std::size_t>{image, 0, 0})
		);
	}

	{
		auto file = lay_out(path.get(), layout);
		run_now(
			*executor,
			make_image_write_operation(
				std::move(file),
				layout,
				source.share_const(),
				regions,
				6 * sizeof(float)
			)
		);
	}

	CHECK(
		values_of<float>(path.get(), 36) ==
		std::vector<float>(values.begin(), values.end())
	);
}

TEST_CASE(
	"make_image_write_operation writes what a layout lays out",
	"[make_image_write_operation]"
)
{
	const auto executor =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {3, 4};
	const auto values = counting<float>(0, 12);
	const auto source =
		make_host_array<float>(extents, numerical_type::float32, values);

	SECTION( "along strides that are not descending" )
	{
		// The file stores its columns one after another, so the element of
		// row r and column c goes to r + 3 c.
		const auto layout = make_layout(
			extents,
			numerical_type::float32,
			std::vector<std::ptrdiff_t>{1, 3}
		);
		{
			auto file = lay_out(path.get(), layout);
			run_now(
				*executor,
				make_image_write_operation(
					std::move(file),
					layout,
					source.share_const(),
					whole_of(extents),
					1 << 20
				)
			);
		}

		std::vector<float> expected(12);
		for (std::size_t row = 0; row < 3; ++row)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				expected[row + 3 * column] = values[row * 4 + column];
			}
		}
		CHECK( values_of<float>(path.get(), 12) == expected );
	}

	SECTION( "in the other byte order" )
	{
		const auto other =
			get_system_byte_order() == byte_order::little_endian
				? byte_order::big_endian
				: byte_order::little_endian;
		const auto layout = make_layout(
			extents,
			numerical_type::float32,
			contiguous_strides(extents),
			other
		);
		{
			auto file = lay_out(path.get(), layout);
			run_now(
				*executor,
				make_image_write_operation(
					std::move(file),
					layout,
					source.share_const(),
					whole_of(extents),
					1 << 20
				)
			);
		}

		auto stored = values_of<float>(path.get(), 12);
		auto *bytes = reinterpret_cast<char*>(stored.data());
		for (std::size_t i = 0; i < stored.size(); ++i)
		{
			std::reverse(
				bytes + i * sizeof(float),
				bytes + (i + 1) * sizeof(float)
			);
		}
		CHECK( stored == values );
	}
}

TEST_CASE(
	"make_image_write_operation leaves what lies between the regions of a "
	"write as it was",
	"[make_image_write_operation]"
)
{
	const auto executor =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {4, 4};
	const auto layout = make_layout(
		extents,
		numerical_type::int16,
		contiguous_strides(extents)
	);

	// A patch of two rows of two values, from the second row and column
	// on, converted from float32.
	const auto source = make_host_array<float>(
		{2, 2},
		numerical_type::float32,
		std::vector<float>{1, 2, 3, 4}
	);
	image_transfer_plan regions(image_transfer_shape({2, 2}, 2, 2));
	regions.add(
		make_span(std::vector<std::size_t>{1, 1}),
		make_span(std::vector<std::size_t>{0, 0})
	);

	{
		auto file = lay_out(path.get(), layout, '\0');
		run_now(
			*executor,
			make_image_write_operation(
				std::move(file),
				layout,
				source.share_const(),
				regions,
				1 << 20
			)
		);
	}

	CHECK(
		values_of<std::int16_t>(path.get(), 16) ==
		std::vector<std::int16_t>{
			0, 0, 0, 0,
			0, 1, 2, 0,
			0, 3, 4, 0,
			0, 0, 0, 0,
		}
	);
}

TEST_CASE(
	"writes of regions that do not overlap may be under way at once",
	"[make_image_write_operation]"
)
{
	const auto executor =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {8, 4, 4};
	const auto layout = make_layout(
		extents,
		numerical_type::float32,
		contiguous_strides(extents)
	);
	const auto values = counting<double>(0, 128);
	const auto source =
		make_host_array<double>(extents, numerical_type::float64, values);

	{
		auto file = lay_out(path.get(), layout);
		const auto done = std::make_shared<counting_completion>(8);
		for (std::size_t image = 0; image < 8; ++image)
		{
			image_transfer_plan regions(image_transfer_shape({4, 4}, 3, 3));
			regions.add(
				make_span(std::vector<std::size_t>{image, 0, 0}),
				make_span(std::vector<std::size_t>{image, 0, 0})
			);
			executor->submit(
				make_image_write_operation(
					file,
					layout,
					source.share_const(),
					regions,
					1 << 20
				),
				done
			);
		}
		done->get();
	}

	CHECK(
		values_of<float>(path.get(), 128) ==
		std::vector<float>(values.begin(), values.end())
	);
}

TEST_CASE(
	"make_image_write_operation refuses regions it can not write",
	"[make_image_write_operation]"
)
{
	const scoped_path path(name_file());
	const std::vector<std::size_t> extents = {3, 4};
	const auto layout = make_layout(
		extents,
		numerical_type::float32,
		contiguous_strides(extents)
	);
	const auto file = lay_out(path.get(), layout);

	SECTION( "an uninitialized source" )
	{
		REQUIRE_THROWS_AS(
			make_image_write_operation(
				file,
				layout,
				const_array(),
				whole_of(extents),
				1 << 20
			),
			std::invalid_argument
		);
	}

	SECTION( "a region the file does not contain" )
	{
		const auto source =
			make_host_array<float>(extents, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		REQUIRE_THROWS_AS(
			make_image_write_operation(
				file,
				layout,
				source.share_const(),
				regions,
				1 << 20
			),
			std::out_of_range
		);
	}
}
