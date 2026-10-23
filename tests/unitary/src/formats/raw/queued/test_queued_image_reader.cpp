// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/queued_image_reader.hpp>

#include "../../../fixtures/file_transfer_executors.hpp"

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// What every file below begins with, before its values, unless a case says
// otherwise.
const std::size_t preamble_size = 16;

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i);
	}

	return values;
}

template <typename T>
void write_file(
	const std::string &path,
	const std::vector<T> &values,
	std::size_t preamble = preamble_size
)
{
	std::vector<char> raw(preamble + values.size() * sizeof(T), 'p');
	std::memcpy(
		raw.data() + preamble,
		values.data(),
		values.size() * sizeof(T)
	);

	std::ofstream output(path, std::ios::out | std::ios::binary);
	output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

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
	numerical_type data_type = numerical_type::float32,
	std::vector<std::ptrdiff_t> strides = {},
	std::size_t preamble = preamble_size,
	byte_order order = get_system_byte_order()
)
{
	return raw_image_layout(
		image_descriptor(make_span(extents), 2, data_type),
		strides.empty() ? contiguous_strides(extents) : std::move(strides),
		preamble,
		order
	);
}

std::vector<float> read_all(
	const queued_image_reader &reader,
	const std::vector<std::size_t> &extents
)
{
	auto destination = make_host_array<float>(
		extents,
		numerical_type::float32,
		-1.0F
	);
	read_now(reader, destination, whole_of(extents));

	return get_values<float>(destination);
}

} // anonymous namespace

TEST_CASE(
	"a queued_image_reader reports what it was opened with",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_descriptor.raw");
	const std::vector<std::size_t> extents = {2, 3, 4};
	write_file(path.get(), counting(24));

	const queued_image_reader reader(
		path.get(),
		make_layout(extents),
		image_metadata(),
		transfers
	);

	CHECK( reader.get_descriptor() ==
		image_descriptor(make_span(extents), 2, numerical_type::float32) );
}

TEST_CASE(
	"a queued_image_reader reads the values behind the preamble",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_values.raw");
	const std::vector<std::size_t> extents = {3, 3, 4};
	const auto values = counting(36);
	write_file(path.get(), values);

	const queued_image_reader reader(
		path.get(),
		make_layout(extents),
		image_metadata(),
		transfers
	);

	SECTION( "the whole of a stack arrives in order" )
	{
		CHECK( read_all(reader, extents) == values );
	}

	SECTION( "one plane of a stack is read on its own" )
	{
		auto destination =
			make_host_array<float>({3, 4}, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape({3, 4}, 3, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		read_now(reader, destination, regions);

		CHECK( get_values<float>(destination) ==
			std::vector<float>(values.begin() + 12, values.begin() + 24) );
	}

	SECTION( "regions stated out of order all arrive where they belong" )
	{
		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape({3, 4}, 3, 3));
		const std::array<std::size_t, 3> sections = {2, 0, 1};
		for (std::size_t i = 0; i < sections.size(); ++i)
		{
			regions.add(
				make_span(std::vector<std::size_t>{sections[i], 0, 0}),
				make_span(std::vector<std::size_t>{i, 0, 0})
			);
		}

		read_now(reader, destination, regions);

		std::vector<float> expected;
		for (const auto section : sections)
		{
			const auto first =
				values.begin() + static_cast<std::ptrdiff_t>(section * 12);
			expected.insert(expected.end(), first, first + 12);
		}
		CHECK( get_values<float>(destination) == expected );
	}

	SECTION( "an empty plan reads nothing and succeeds" )
	{
		auto destination =
			make_host_array<float>({3, 4}, numerical_type::float32);
		const image_transfer_plan regions(image_transfer_shape({3, 4}, 3, 2));

		CHECK_NOTHROW( read_now(reader, destination, regions) );
	}

	SECTION( "reads from several threads at once each get their values" )
	{
		std::vector<std::vector<float>> planes(3);
		std::vector<std::thread> threads;
		for (std::size_t plane = 0; plane < planes.size(); ++plane)
		{
			threads.emplace_back([&reader, &planes, plane]
			{
				auto destination =
					make_host_array<float>({3, 4}, numerical_type::float32);
				image_transfer_plan regions(image_transfer_shape({3, 4}, 3, 2));
				regions.add(
					make_span(std::vector<std::size_t>{plane, 0, 0}),
					make_span(std::vector<std::size_t>{0, 0})
				);

				for (int round = 0; round < 50; ++round)
				{
					read_now(reader, destination, regions);
				}
				planes[plane] = get_values<float>(destination);
			});
		}
		for (auto &thread : threads)
		{
			thread.join();
		}

		for (std::size_t plane = 0; plane < planes.size(); ++plane)
		{
			const auto first =
				values.begin() + static_cast<std::ptrdiff_t>(plane * 12);
			CHECK( planes[plane] == std::vector<float>(first, first + 12) );
		}
	}
}

TEST_CASE(
	"a queued_image_reader reads its values as the layout states them",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_layout.raw");
	const std::vector<std::size_t> extents = {3, 4};

	SECTION( "along strides that are not descending" )
	{
		// The file stores its columns one after another, so the element of
		// row r and column c is the one at r + 3 c.
		const auto stored = counting(12);
		write_file(path.get(), stored);

		const queued_image_reader reader(
			path.get(),
			make_layout(extents, numerical_type::float32, {1, 3}),
			image_metadata(),
			transfers
		);

		std::vector<float> expected;
		for (std::size_t row = 0; row < 3; ++row)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				expected.push_back(stored[row + 3 * column]);
			}
		}
		CHECK( read_all(reader, extents) == expected );
	}

	SECTION( "in the other byte order" )
	{
		auto swapped = counting(12);
		auto *bytes = reinterpret_cast<char*>(swapped.data());
		for (std::size_t i = 0; i < swapped.size(); ++i)
		{
			std::reverse(
				bytes + i * sizeof(float),
				bytes + (i + 1) * sizeof(float)
			);
		}
		write_file(path.get(), swapped);

		const auto other =
			get_system_byte_order() == byte_order::little_endian
				? byte_order::big_endian
				: byte_order::little_endian;
		const queued_image_reader reader(
			path.get(),
			make_layout(
				extents,
				numerical_type::float32,
				{},
				preamble_size,
				other
			),
			image_metadata(),
			transfers
		);

		CHECK( read_all(reader, extents) == counting(12) );
	}

	SECTION( "converted to the data type of the array" )
	{
		const std::vector<std::int16_t> stored = {
			-2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 300
		};
		write_file(path.get(), stored);

		const queued_image_reader reader(
			path.get(),
			make_layout(extents, numerical_type::int16),
			image_metadata(),
			transfers
		);

		CHECK( read_all(reader, extents) ==
			std::vector<float>(stored.cbegin(), stored.cend()) );
	}

	SECTION( "that are not aligned for their data type" )
	{
		write_file(path.get(), counting(12), preamble_size + 2);

		const queued_image_reader reader(
			path.get(),
			make_layout(
				extents,
				numerical_type::float32,
				{},
				preamble_size + 2
			),
			image_metadata(),
			transfers
		);

		CHECK( read_all(reader, extents) == counting(12) );
	}
}

TEST_CASE(
	"a queued_image_reader refuses what it can not read",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_refused.raw");
	const std::vector<std::size_t> extents = {3, 4};

	SECTION( "an uninitialized destination" )
	{
		write_file(path.get(), counting(12));
		const queued_image_reader reader(
			path.get(),
			make_layout(extents),
			image_metadata(),
			transfers
		);

		array uninitialized;
		CHECK_THROWS_AS(
			read_now(reader, uninitialized, whole_of(extents)),
			std::invalid_argument
		);
	}

	SECTION( "a region the file does not contain" )
	{
		write_file(path.get(), counting(12));
		const queued_image_reader reader(
			path.get(),
			make_layout(extents),
			image_metadata(),
			transfers
		);

		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		CHECK_THROWS_AS(
			read_now(reader, destination, regions),
			std::out_of_range
		);
	}

	SECTION( "a data type the array's can not be produced from" )
	{
		write_file(path.get(), std::vector<std::complex<float>>(12));
		const queued_image_reader reader(
			path.get(),
			make_layout(extents, numerical_type::complex_float32),
			image_metadata(),
			transfers
		);

		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		CHECK_THROWS_AS(
			read_now(reader, destination, whole_of(extents)),
			unsupported_operation_error
		);
	}
}

TEST_CASE(
	"a queued_image_reader refuses a file that can not hold its layout, "
	"naming it",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_bad_file.raw");
	const std::vector<std::size_t> extents = {2, 3, 4};
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path.get() + ": ")
	);

	SECTION( "one shorter than the values it is said to hold" )
	{
		write_file(path.get(), counting(23));

		CHECK_THROWS_MATCHES(
			queued_image_reader(
				path.get(),
				make_layout(extents),
				image_metadata(),
				transfers
			),
			image_file_format_error,
			names_the_file
		);
	}

	SECTION( "one that is not there" )
	{
		CHECK_THROWS_MATCHES(
			queued_image_reader(
				path.get(),
				make_layout(extents),
				image_metadata(),
				transfers
			),
			image_file_error,
			names_the_file
		);
	}
}

TEST_CASE(
	"a queued_image_reader refuses a null notifier",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_null_notifier.raw");
	const std::vector<std::size_t> extents = {3, 4};
	write_file(path.get(), counting(12));

	const queued_image_reader reader(
		path.get(),
		make_layout(extents),
		image_metadata(),
		transfers
	);

	auto destination = make_host_array<float>(extents, numerical_type::float32);
	CHECK_THROWS_AS(
		reader.read_async(destination.share(), whole_of(extents), nullptr),
		std::invalid_argument
	);
}

TEST_CASE(
	"a queued_image_reader reports what it can not plan, rather than "
	"throwing it",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_reported.raw");
	const std::vector<std::size_t> extents = {3, 4};
	write_file(path.get(), counting(12));

	const queued_image_reader reader(
		path.get(),
		make_layout(extents),
		image_metadata(),
		transfers
	);

	auto destination = make_host_array<float>(extents, numerical_type::float32);
	image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
	regions.add(
		make_span(std::vector<std::size_t>{1, 0}),
		make_span(std::vector<std::size_t>{0, 0})
	);

	const auto done = std::make_shared<counting_completion>(1);
	CHECK_NOTHROW( reader.read_async(destination.share(), regions, done) );
	REQUIRE( done->is_ready() );
	CHECK_THROWS_AS( done->get(), std::out_of_range );
}

TEST_CASE(
	"a read of a queued_image_reader goes on after the reader is gone",
	"[queued_image_reader]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("queued_reader_gone.raw");
	const std::vector<std::size_t> extents = {64, 3, 4};
	const auto values = counting(64 * 12);
	write_file(path.get(), values);

	// Converted, so that the read takes tasks as well as reads.
	auto destination =
		make_host_array<double>(extents, numerical_type::float64, -1.0);
	const auto done = std::make_shared<counting_completion>(1);
	{
		const queued_image_reader reader(
			path.get(),
			make_layout(extents),
			image_metadata(),
			transfers
		);
		reader.read_async(destination.share(), whole_of(extents), done);
	}

	CHECK_NOTHROW( done->get() );
	CHECK( get_values<double>(destination) ==
		std::vector<double>(values.begin(), values.end()) );
}
