// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_read_format_manager.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/image_write_format_manager.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i) + 0.25F;
	}

	return values;
}

} // anonymous namespace

TEST_CASE(
	"a TIFF file created through the managers reads back as it was written",
	"[tiff][image_format_manager]"
)
{
	image_write_format_manager writers;
	writers.register_builtin_formats();
	image_read_format_manager readers;
	readers.register_builtin_formats();

	// The two shapes a TIFF file can hold, under both of its extensions.
	const std::vector<std::vector<std::size_t>> shapes = {
		{3, 4},
		{2, 3, 4}
	};
	const std::vector<std::string> names = {
		"round_trip_managers.tif",
		"round_trip_managers.tiff"
	};

	for (const auto &name : names)
	{
		for (const auto &extents : shapes)
		{
			const scoped_path path(name);
			const auto values = counting(count_elements(extents));

			auto source =
				make_host_array<float>(extents, numerical_type::float32);
			std::memcpy(
				source.get_data(),
				values.data(),
				values.size() * sizeof(float)
			);

			const image_descriptor descriptor(
				make_span(extents),
				2,
				numerical_type::float32
			);

			{
				const auto writer =
					writers.open(path.get(), descriptor, image_metadata());
				writer->write(const_array_ref(source), whole_of(extents));
				writer->flush();
			}

			const auto reader = readers.open(path.get());

			REQUIRE( reader->get_descriptor() == descriptor );

			auto destination =
				make_host_array<float>(extents, numerical_type::float32);
			reader->read(array_ref(destination), whole_of(extents));

			REQUIRE( get_values<float>(destination) ==
				values );
		}
	}
}

TEST_CASE(
	"a TIFF file converts to and from the type it holds",
	"[tiff][image_format_manager]"
)
{
	image_write_format_manager writers;
	writers.register_builtin_formats();
	image_read_format_manager readers;
	readers.register_builtin_formats();

	const std::vector<std::size_t> extents = {2, 2};
	const std::vector<float> values = {0.0F, 1.0F, 100.0F, 127.0F};

	const numerical_type file_types[] = {
		numerical_type::int8,
		numerical_type::uint8,
		numerical_type::int16,
		numerical_type::uint16,
		numerical_type::int32,
		numerical_type::uint32,
		numerical_type::int64,
		numerical_type::uint64,
		numerical_type::float16,
		numerical_type::float32,
		numerical_type::float64
	};

	for (const auto file_type : file_types)
	{
		const scoped_path path("round_trip_conversion.tif");

		auto source = make_host_array<float>(extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		{
			const auto writer = writers.open(
				path.get(),
				image_descriptor(make_span(extents), 2, file_type),
				image_metadata()
			);
			writer->write(const_array_ref(source), whole_of(extents));
			writer->flush();
		}

		const auto reader = readers.open(path.get());

		REQUIRE( reader->get_descriptor().get_data_type() == file_type );

		// Read back into a wider type than the file holds, which is the
		// conversion a caller asks for rather than the one the file forces.
		auto destination =
			make_host_array<double>(extents, numerical_type::float64);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<double>(destination) ==
			std::vector<double>(values.begin(), values.end()) );
	}
}

TEST_CASE(
	"a TIFF stack is written a page at a time and read back whole",
	"[tiff][image_format_manager]"
)
{
	image_write_format_manager writers;
	writers.register_builtin_formats();
	image_read_format_manager readers;
	readers.register_builtin_formats();

	const scoped_path path("round_trip_paged.tif");
	const std::vector<std::size_t> extents = {4, 3, 5};
	const std::vector<std::size_t> page = {3, 5};
	const auto values = counting(count_elements(extents));

	auto source = make_host_array<float>(extents, numerical_type::float32);
	std::memcpy(
		source.get_data(),
		values.data(),
		values.size() * sizeof(float)
	);

	{
		const auto writer = writers.open(
			path.get(),
			image_descriptor(make_span(extents), 2, numerical_type::int16),
			image_metadata()
		);

		for (std::size_t index = 0; index < extents[0]; ++index)
		{
			image_transfer_plan regions(image_transfer_shape(page, 3, 3));
			regions.add(
				make_span(std::vector<std::size_t>{index, 0, 0}),
				make_span(std::vector<std::size_t>{index, 0, 0})
			);
			writer->write(const_array_ref(source), regions);
		}
	}

	const auto reader = readers.open(path.get());
	auto destination =
		make_host_array<std::int16_t>(extents, numerical_type::int16);
	reader->read(array_ref(destination), whole_of(extents));

	// The file holds integers, so the quarter every value carries is gone.
	std::vector<std::int16_t> expected(values.size());
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		expected[i] = static_cast<std::int16_t>(i);
	}

	REQUIRE( get_values<std::int16_t>(destination) ==
		expected );
}

TEST_CASE(
	"what a TIFF file can not hold is refused through the managers",
	"[tiff][image_format_manager]"
)
{
	image_write_format_manager writers;
	writers.register_builtin_formats();
	const scoped_path path("round_trip_refused.tif");

	SECTION( "a stack of one image" )
	{
		const std::vector<std::size_t> extents = {1, 3, 4};

		REQUIRE_THROWS_AS(
			writers.open(
				path.get(),
				image_descriptor(
					make_span(extents), 2, numerical_type::float32),
				image_metadata()
			),
			unsupported_operation_error
		);
	}

	SECTION( "a volume" )
	{
		const std::vector<std::size_t> extents = {2, 3, 4};

		REQUIRE_THROWS_AS(
			writers.open(
				path.get(),
				image_descriptor(
					make_span(extents), 3, numerical_type::float32),
				image_metadata()
			),
			unsupported_operation_error
		);
	}

	SECTION( "part of a page" )
	{
		const std::vector<std::size_t> extents = {3, 4};
		const std::vector<std::size_t> half = {3, 2};

		auto source = make_host_array<float>(half, numerical_type::float32);
		const auto writer = writers.open(
			path.get(),
			image_descriptor(make_span(extents), 2, numerical_type::float32),
			image_metadata()
		);

		image_transfer_plan regions(image_transfer_shape(half, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{0, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		REQUIRE_THROWS_AS(
			writer->write(const_array_ref(source), regions),
			unsupported_operation_error
		);
	}
}
