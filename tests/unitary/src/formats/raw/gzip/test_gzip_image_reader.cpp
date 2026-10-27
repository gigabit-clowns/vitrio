// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/gzip/gzip_image_reader.hpp>

#include <formats/raw/raw_image_layout.hpp>

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
#include "../../../compression/zlib_test_streams.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;

namespace
{

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

// The bytes of a file: a preamble of a given size, then the values, in the
// byte order of the host or swapped.
std::vector<byte> lay_out(
	const std::vector<float> &values,
	std::size_t preamble_size,
	bool swapped = false
)
{
	std::vector<byte> raw(preamble_size + values.size() * sizeof(float), 'p');
	std::memcpy(
		raw.data() + preamble_size,
		values.data(),
		values.size() * sizeof(float)
	);

	if (swapped)
	{
		for (auto *value = raw.data() + preamble_size;
			value < raw.data() + raw.size();
			value += sizeof(float))
		{
			std::reverse(value, value + sizeof(float));
		}
	}

	return raw;
}

void write_bytes(const std::string &path, const std::vector<byte> &bytes)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(
		reinterpret_cast<const char*>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

// Compress the bytes of a file into a gzip file.
void write_gzip(const std::string &path, const std::vector<byte> &raw)
{
	write_bytes(path, deflate_stream(raw, 31));
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
	std::size_t core_rank,
	std::size_t preamble_size,
	byte_order order = get_system_byte_order()
)
{
	return raw_image_layout(
		image_descriptor(
			make_span(extents),
			core_rank,
			numerical_type::float32
		),
		contiguous_strides(extents),
		preamble_size,
		order
	);
}

byte_order other_byte_order()
{
	return get_system_byte_order() == byte_order::little_endian
		? byte_order::big_endian
		: byte_order::little_endian;
}

array make_host_array(
	const std::vector<std::size_t> &extents,
	numerical_type type = numerical_type::float32
)
{
	return make_array(make_contiguous_array_descriptor(make_span(extents), type));
}

template <typename T = float>
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
	const gzip_image_reader &reader,
	const std::vector<std::size_t> &extents
)
{
	auto destination = make_host_array(extents);
	reader.read(array_ref(destination), whole_of(extents));
	return values_of(destination, extents);
}

// The patch of rows and columns of an image of a given width.
std::vector<float> patch_of(
	const std::vector<float> &values,
	std::size_t width,
	std::size_t first_row,
	std::size_t rows,
	std::size_t first_column,
	std::size_t columns
)
{
	std::vector<float> patch;
	for (auto row = first_row; row < first_row + rows; ++row)
	{
		for (auto column = first_column;
			column < first_column + columns;
			++column)
		{
			patch.push_back(values[row * width + column]);
		}
	}

	return patch;
}

} // anonymous namespace

TEST_CASE(
	"a gzip_image_reader reports its layout and reads nothing until asked",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_descriptor.raw.gz");
	const std::vector<std::size_t> extents = {2, 3, 4};

	// The file is not there: nothing is read of it yet.
	const gzip_image_reader reader(
		path.get(),
		make_layout(extents, 2, 16),
		image_metadata()
	);

	REQUIRE( reader.get_descriptor() ==
		image_descriptor(make_span(extents), 2, numerical_type::float32) );

	auto destination = make_host_array(extents);
	REQUIRE_THROWS_AS(
		reader.read(array_ref(destination), whole_of(extents)),
		image_file_error
	);
}

TEST_CASE(
	"a gzip_image_reader inflates what lies as it lands straight into place",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_direct.raw.gz");
	const std::vector<std::size_t> extents = {3, 3, 4};
	const auto values = counting(36);

	SECTION( "the whole of a stack arrives in order" )
	{
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		REQUIRE( read_all(reader, extents) == values );
	}

	SECTION( "behind a preamble that leaves the values unaligned" )
	{
		write_gzip(path.get(), lay_out(values, 3));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 3), image_metadata());

		REQUIRE( read_all(reader, extents) == values );
	}

	SECTION( "regions stated out of order arrive where they belong" )
	{
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		auto destination = make_host_array(extents);
		const std::vector<std::size_t> region = {3, 4};
		image_transfer_plan regions(image_transfer_shape(region, 3, 3));
		const std::vector<std::size_t> sections = {2, 0, 1};
		for (std::size_t i = 0; i < sections.size(); ++i)
		{
			regions.add(
				make_span(std::vector<std::size_t>{sections[i], 0, 0}),
				make_span(std::vector<std::size_t>{i, 0, 0})
			);
		}

		reader.read(array_ref(destination), regions);

		std::vector<float> expected;
		for (const auto section : sections)
		{
			const auto first = values.begin() +
				static_cast<std::ptrdiff_t>(section * 12);
			expected.insert(expected.end(), first, first + 12);
		}
		REQUIRE( values_of(destination, extents) == expected );
	}

	SECTION( "a plane read twice arrives in both places" )
	{
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		const std::vector<std::size_t> both = {2, 3, 4};
		auto destination = make_host_array(both);
		const std::vector<std::size_t> region = {3, 4};
		image_transfer_plan regions(image_transfer_shape(region, 3, 3));
		for (std::size_t i = 0; i < 2; ++i)
		{
			regions.add(
				make_span(std::vector<std::size_t>{1, 0, 0}),
				make_span(std::vector<std::size_t>{i, 0, 0})
			);
		}

		reader.read(array_ref(destination), regions);

		std::vector<float> expected(values.begin() + 12, values.begin() + 24);
		expected.insert(expected.end(), expected.begin(), expected.end());
		REQUIRE( values_of(destination, both) == expected );
	}

	SECTION( "a file of several members reads as one" )
	{
		const auto raw = lay_out(values, 16);
		const auto middle = raw.size() / 2;
		auto encoded = deflate_stream(
			std::vector<byte>(raw.begin(), raw.begin() + middle), 31);
		const auto rest = deflate_stream(
			std::vector<byte>(raw.begin() + middle, raw.end()), 31);
		encoded.insert(encoded.end(), rest.begin(), rest.end());
		write_bytes(path.get(), encoded);

		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		REQUIRE( read_all(reader, extents) == values );
	}
}

TEST_CASE(
	"a gzip_image_reader converts what does not lie as it lands",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_staged.raw.gz");
	const std::vector<std::size_t> extents = {6, 5};
	const auto values = counting(30);

	SECTION( "a patch across rows" )
	{
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		const std::vector<std::size_t> region = {3, 2};
		auto destination = make_host_array(region);
		image_transfer_plan regions(image_transfer_shape(region, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{2, 1}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		reader.read(array_ref(destination), regions);

		REQUIRE( values_of(destination, region) ==
			patch_of(values, 5, 2, 3, 1, 2) );
	}

	SECTION( "patches that overlap in the file" )
	{
		write_gzip(path.get(), lay_out(values, 3));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 3), image_metadata());

		const std::vector<std::size_t> region = {2, 5};
		const std::vector<std::size_t> both = {2, 2, 5};
		auto destination = make_host_array(both);
		image_transfer_plan regions(image_transfer_shape(region, 2, 3));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);
		regions.add(
			make_span(std::vector<std::size_t>{2, 0}),
			make_span(std::vector<std::size_t>{1, 0, 0})
		);

		reader.read(array_ref(destination), regions);

		auto expected = patch_of(values, 5, 1, 2, 0, 5);
		const auto second = patch_of(values, 5, 2, 2, 0, 5);
		expected.insert(expected.end(), second.begin(), second.end());
		REQUIRE( values_of(destination, both) == expected );
	}

	SECTION( "into another data type" )
	{
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		auto destination = make_host_array(extents, numerical_type::float64);
		reader.read(array_ref(destination), whole_of(extents));

		REQUIRE( values_of<double>(destination, extents) ==
			std::vector<double>(values.begin(), values.end()) );
	}

	SECTION( "from the other byte order, behind an unaligned preamble" )
	{
		write_gzip(path.get(), lay_out(values, 5, true));
		const gzip_image_reader reader(
			path.get(),
			make_layout(extents, 2, 5, other_byte_order()),
			image_metadata()
		);

		REQUIRE( read_all(reader, extents) == values );
	}

	SECTION( "a slice across the planes of a volume" )
	{
		// A row of each plane, which the reader stages a plane at a time
		// rather than with everything that lies between them.
		const std::vector<std::size_t> volume = {4, 64, 64};
		const auto many = counting(4 * 64 * 64);
		write_gzip(path.get(), lay_out(many, 16));
		const gzip_image_reader reader(
			path.get(), make_layout(volume, 3, 16), image_metadata());

		const std::vector<std::size_t> slice = {4, 1, 64};
		auto destination = make_host_array(slice);
		image_transfer_plan regions(image_transfer_shape(slice, 3, 3));
		regions.add(
			make_span(std::vector<std::size_t>{0, 10, 0}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);

		reader.read(array_ref(destination), regions);

		std::vector<float> expected;
		for (std::size_t plane = 0; plane < 4; ++plane)
		{
			const auto first = many.begin() +
				static_cast<std::ptrdiff_t>(plane * 64 * 64 + 10 * 64);
			expected.insert(expected.end(), first, first + 64);
		}
		REQUIRE( values_of(destination, slice) == expected );
	}

	SECTION( "along strides that are not descending" )
	{
		// The file stores its columns one after another, so the element of
		// row r and column c is the one at r + 6 c.
		write_gzip(path.get(), lay_out(values, 16));
		const gzip_image_reader reader(
			path.get(),
			raw_image_layout(
				image_descriptor(
					make_span(extents), 2, numerical_type::float32),
				std::vector<std::ptrdiff_t>{1, 6},
				16,
				get_system_byte_order()
			),
			image_metadata()
		);

		std::vector<float> expected;
		for (std::size_t row = 0; row < 6; ++row)
		{
			for (std::size_t column = 0; column < 5; ++column)
			{
				expected.push_back(values[row + 6 * column]);
			}
		}
		REQUIRE( read_all(reader, extents) == expected );
	}
}

TEST_CASE(
	"a gzip_image_reader inflates no further than a read needs",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_short.raw.gz");
	const std::vector<std::size_t> extents = {4, 64, 64};
	const auto values = counting(4 * 64 * 64);

	// The stream ends partway through the second plane.
	auto raw = lay_out(values, 16);
	raw.resize(16 + (64 * 64 + 100) * sizeof(float));
	write_gzip(path.get(), raw);

	const gzip_image_reader reader(
		path.get(), make_layout(extents, 2, 16), image_metadata());

	const std::vector<std::size_t> region = {64, 64};
	auto destination = make_host_array(region);
	const auto plane = [](std::size_t section)
	{
		image_transfer_plan regions(
			image_transfer_shape(std::vector<std::size_t>{64, 64}, 3, 2));
		regions.add(
			make_span(std::vector<std::size_t>{section, 0, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);
		return regions;
	};

	SECTION( "what lies before the end is read" )
	{
		reader.read(array_ref(destination), plane(0));
		REQUIRE( values_of(destination, region) ==
			std::vector<float>(values.begin(), values.begin() + 64 * 64) );
	}

	SECTION( "what lies past it is refused" )
	{
		REQUIRE_THROWS_WITH(
			reader.read(array_ref(destination), plane(1)),
			Catch::Matchers::StartsWith(path.get()) &&
			Catch::Matchers::ContainsSubstring("ends before")
		);
	}
}

TEST_CASE(
	"a gzip_image_reader refuses what it can not read",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_refused.raw.gz");
	const std::vector<std::size_t> extents = {2, 3};

	SECTION( "a file that is not gzip" )
	{
		write_bytes(path.get(), lay_out(counting(6), 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		auto destination = make_host_array(extents);
		REQUIRE_THROWS_AS(
			reader.read(array_ref(destination), whole_of(extents)),
			image_file_format_error
		);
	}

	SECTION( "an empty plan reads nothing, not even the file" )
	{
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		auto destination = make_host_array(extents);
		const image_transfer_plan regions(
			image_transfer_shape(extents, 2, 2));
		REQUIRE_NOTHROW( reader.read(array_ref(destination), regions) );
	}

	SECTION( "a region outside of the image" )
	{
		write_gzip(path.get(), lay_out(counting(6), 16));
		const gzip_image_reader reader(
			path.get(), make_layout(extents, 2, 16), image_metadata());

		auto destination = make_host_array(extents);
		image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);
		REQUIRE_THROWS_AS(
			reader.read(array_ref(destination), regions),
			std::out_of_range
		);
	}
}

TEST_CASE(
	"a gzip_image_reader may be read from several threads at once",
	"[gzip_image_reader]"
)
{
	const scoped_path path("gzip_reader_threads.raw.gz");
	const std::vector<std::size_t> extents = {8, 32, 32};
	const auto values = counting(8 * 32 * 32);
	write_gzip(path.get(), lay_out(values, 16));

	const gzip_image_reader reader(
		path.get(), make_layout(extents, 2, 16), image_metadata());

	std::vector<std::vector<float>> results(4);
	std::vector<std::thread> threads;
	for (auto &result : results)
	{
		threads.emplace_back([&reader, &extents, &result]
		{
			result = read_all(reader, extents);
		});
	}
	for (auto &thread : threads)
	{
		thread.join();
	}

	for (const auto &result : results)
	{
		REQUIRE( result == values );
	}
}
