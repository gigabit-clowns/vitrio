// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/memory_mapping/unaligned_mapped_image_reader.hpp>

#include <formats/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <memory/byte_order.hpp>

#include <vitrio/tests/scoped_path.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;

namespace
{

// What every file below begins with, before its values. Odd, so that no
// element type wider than a byte is aligned behind it.
const std::size_t preamble_size = 13;

std::size_t element_count(const std::vector<std::size_t> &extents)
{
	return std::accumulate(
		extents.cbegin(),
		extents.cend(),
		std::size_t(1),
		std::multiplies<std::size_t>()
	);
}

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i);
	}

	return values;
}

// Write a preamble followed by the bytes of some float32 values, each in
// the given byte order.
void write_file(
	const std::string &path,
	const std::vector<float> &values,
	byte_order order = get_system_byte_order()
)
{
	std::vector<char> raw(preamble_size + values.size() * sizeof(float), 'p');
	std::memcpy(
		raw.data() + preamble_size,
		values.data(),
		values.size() * sizeof(float)
	);
	if (order != get_system_byte_order())
	{
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			auto *first = raw.data() + preamble_size + i * sizeof(float);
			std::reverse(first, first + sizeof(float));
		}
	}

	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(raw.data(), static_cast<std::streamsize>(raw.size()));
}

image_descriptor make_descriptor(
	const std::vector<std::size_t> &extents,
	numerical_type data_type = numerical_type::float32
)
{
	return image_descriptor(make_span(extents), 2, data_type);
}

std::unique_ptr<const unaligned_mapped_image_reader> open_reader(
	const std::string &path,
	const std::vector<std::size_t> &extents,
	byte_order order = get_system_byte_order()
)
{
	return std::unique_ptr<const unaligned_mapped_image_reader>(
		new unaligned_mapped_image_reader(
			path,
			make_descriptor(extents),
			preamble_size,
			order,
			image_metadata()
		)
	);
}

array make_host_array(
	const std::vector<std::size_t> &extents,
	numerical_type data_type = numerical_type::float32
)
{
	return make_array(
		make_contiguous_array_descriptor(make_span(extents), data_type)
	);
}

template <typename T>
std::vector<T> values_of(
	const array &destination,
	const std::vector<std::size_t> &extents
)
{
	const auto *data = reinterpret_cast<const T*>(destination.get_data());

	return std::vector<T>(data, data + element_count(extents));
}

image_transfer_plan whole_of(const std::vector<std::size_t> &extents)
{
	image_transfer_plan regions(
		image_transfer_shape(extents, extents.size(), extents.size())
	);
	regions.add(
		make_span(std::vector<std::size_t>(extents.size(), 0)),
		make_span(std::vector<std::size_t>(extents.size(), 0))
	);

	return regions;
}

std::vector<float> read_all(
	const unaligned_mapped_image_reader &reader,
	const std::vector<std::size_t> &extents
)
{
	auto destination = make_host_array(extents);
	reader.read(array_ref(destination), whole_of(extents));

	return values_of<float>(destination, extents);
}

} // anonymous namespace

TEST_CASE(
	"an unaligned_mapped_image_reader reports what it was given",
	"[unaligned_mapped_image_reader]"
)
{
	const scoped_path path("unaligned_reader_descriptor.raw");
	const std::vector<std::size_t> extents = {2, 3, 4};
	write_file(path.get(), counting(24));

	const auto reader = open_reader(path.get(), extents);

	REQUIRE( reader->get_descriptor() == make_descriptor(extents) );
}

TEST_CASE(
	"an unaligned_mapped_image_reader reads values that are not aligned",
	"[unaligned_mapped_image_reader]"
)
{
	const scoped_path path("unaligned_reader_values.raw");

	SECTION( "the whole of an image arrives in order" )
	{
		const std::vector<std::size_t> extents = {3, 4};
		write_file(path.get(), counting(12));

		REQUIRE( read_all(*open_reader(path.get(), extents), extents) ==
			counting(12) );
	}

	SECTION( "the whole of a stack arrives in order" )
	{
		const std::vector<std::size_t> extents = {3, 4, 5};
		write_file(path.get(), counting(60));

		REQUIRE( read_all(*open_reader(path.get(), extents), extents) ==
			counting(60) );
	}

	SECTION( "a few rows of one page of a stack" )
	{
		const std::vector<std::size_t> extents = {3, 4, 5};
		write_file(path.get(), counting(60));
		const auto reader = open_reader(path.get(), extents);

		// Rows 1 and 2, columns 1 to 3, of the page 2.
		const std::vector<std::size_t> shape = {1, 2, 3};
		auto destination = make_host_array(shape);
		image_transfer_plan regions(image_transfer_shape(shape, 3, 3));
		regions.add(
			make_span(std::vector<std::size_t>{2, 1, 1}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);

		reader->read(array_ref(destination), regions);

		REQUIRE( values_of<float>(destination, shape) ==
			std::vector<float>{46, 47, 48, 51, 52, 53} );
	}

	SECTION( "regions across pages, in the order of the file or not" )
	{
		const std::vector<std::size_t> extents = {5, 2, 3};
		write_file(path.get(), counting(30));
		const auto reader = open_reader(path.get(), extents);

		// The last row of the pages 3 and 4, then of the pages 0 and 1.
		const std::vector<std::size_t> shape = {2, 1, 3};
		const std::vector<std::size_t> array_extents = {4, 1, 3};
		auto destination = make_host_array(array_extents);
		image_transfer_plan regions(image_transfer_shape(shape, 3, 3));
		regions.add(
			make_span(std::vector<std::size_t>{3, 1, 0}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);
		regions.add(
			make_span(std::vector<std::size_t>{0, 1, 0}),
			make_span(std::vector<std::size_t>{2, 0, 0})
		);

		reader->read(array_ref(destination), regions);

		REQUIRE( values_of<float>(destination, array_extents) ==
			std::vector<float>{21, 22, 23, 27, 28, 29, 3, 4, 5, 9, 10, 11} );
	}

	SECTION( "in the other byte order" )
	{
		const std::vector<std::size_t> extents = {3, 4};
		const auto other =
			get_system_byte_order() == byte_order::little_endian
				? byte_order::big_endian
				: byte_order::little_endian;
		write_file(path.get(), counting(12), other);

		REQUIRE( read_all(*open_reader(path.get(), extents, other), extents)
			== counting(12) );
	}

	SECTION( "converted to the data type of the array" )
	{
		const std::vector<std::size_t> extents = {2, 3};
		write_file(path.get(), counting(6));
		const auto reader = open_reader(path.get(), extents);

		auto destination = make_host_array(extents, numerical_type::float64);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( values_of<double>(destination, extents) ==
			std::vector<double>{0, 1, 2, 3, 4, 5} );
	}

	SECTION( "an empty plan reads nothing and succeeds" )
	{
		const std::vector<std::size_t> extents = {3, 4};
		write_file(path.get(), counting(12));
		const auto reader = open_reader(path.get(), extents);

		auto destination = make_host_array(extents);
		const image_transfer_plan regions(
			image_transfer_shape(extents, 2, 2)
		);

		REQUIRE_NOTHROW( reader->read(array_ref(destination), regions) );
	}
}

TEST_CASE(
	"an unaligned_mapped_image_reader is read from several threads at once",
	"[unaligned_mapped_image_reader]"
)
{
	const scoped_path path("unaligned_reader_concurrent.raw");
	const std::vector<std::size_t> extents = {8, 16, 16};
	write_file(path.get(), counting(element_count(extents)));
	const auto reader = open_reader(path.get(), extents);

	std::vector<std::vector<float>> results(4);
	std::vector<std::thread> threads;
	for (auto &result : results)
	{
		threads.emplace_back([&reader, &result, &extents] ()
		{
			result = read_all(*reader, extents);
		});
	}
	for (auto &thread : threads)
	{
		thread.join();
	}

	for (const auto &result : results)
	{
		REQUIRE( result == counting(element_count(extents)) );
	}
}

TEST_CASE(
	"an unaligned_mapped_image_reader refuses what it can not read",
	"[unaligned_mapped_image_reader]"
)
{
	const scoped_path path("unaligned_reader_refused.raw");
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path.get() + ": ")
	);

	SECTION( "values of neither two nor three axes" )
	{
		write_file(path.get(), counting(16));
		const std::vector<std::size_t> extents = {2, 2, 2, 2};

		REQUIRE_THROWS_AS(
			open_reader(path.get(), extents),
			std::invalid_argument
		);
	}

	SECTION( "a file shorter than the values it is said to hold" )
	{
		write_file(path.get(), counting(11));

		REQUIRE_THROWS_MATCHES(
			open_reader(path.get(), {3, 4}),
			image_file_format_error,
			names_the_file
		);
	}

	SECTION( "a file whose values would not fit in memory" )
	{
		write_file(path.get(), counting(12));
		const std::vector<std::size_t> extents = {
			std::size_t(1) << 40, std::size_t(1) << 40
		};

		REQUIRE_THROWS_AS(
			open_reader(path.get(), extents),
			image_file_format_error
		);
	}

	SECTION( "a file that is not there" )
	{
		REQUIRE_THROWS_MATCHES(
			open_reader(path.get(), {3, 4}),
			image_file_error,
			names_the_file
		);
	}

	SECTION( "a region the file does not contain" )
	{
		write_file(path.get(), counting(12));
		const std::vector<std::size_t> extents = {3, 4};
		const auto reader = open_reader(path.get(), extents);

		auto destination = make_host_array(extents);
		image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		REQUIRE_THROWS_AS(
			reader->read(array_ref(destination), regions),
			std::out_of_range
		);
	}
}

TEST_CASE(
	"an unaligned_mapped_image_reader supports images and stacks of them "
	"laid out one value after another",
	"[unaligned_mapped_image_reader]"
)
{
	const auto layout_of = [](
		std::vector<std::size_t> extents,
		std::vector<std::ptrdiff_t> strides
	)
	{
		return raw_image_layout(
			image_descriptor(make_span(extents), 2, numerical_type::float32),
			std::move(strides),
			18,
			get_system_byte_order()
		);
	};

	CHECK( unaligned_mapped_image_reader::supports(
		layout_of({3, 4}, {4, 1})
	) );
	CHECK( unaligned_mapped_image_reader::supports(
		layout_of({2, 3, 4}, {12, 4, 1})
	) );
	CHECK_FALSE( unaligned_mapped_image_reader::supports(
		layout_of({2, 3, 4}, {12, 1, 3})
	) );
	CHECK_FALSE( unaligned_mapped_image_reader::supports(
		layout_of({2, 2, 3, 4}, {24, 12, 4, 1})
	) );
}
