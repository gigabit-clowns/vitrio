// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_read_format_selector.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/image_write_format_selector.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <array>
#include <cstdio>
#include <cstring>
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
	"an MRC file created through the selectors reads back as it was written",
	"[mrc][image_format_selector]"
)
{
	image_write_format_selector writers;
	writers.register_builtin_formats();
	image_read_format_selector readers;
	readers.register_builtin_formats();

	// The four shapes an MRC file can hold. The extents of the stack and of
	// the volume are the same, so what tells them apart is the core rank
	// alone, which is the distinction the whole round trip is here to check.
	struct shape
	{
		std::vector<std::size_t> extents;
		std::size_t core_rank;
	};

	const std::vector<shape> shapes = {
		{{3, 4}, 2},
		{{2, 3, 4}, 2},
		{{2, 3, 4}, 3},
		{{2, 3, 4, 5}, 3}
	};

	for (const auto &subject : shapes)
	{
		const scoped_path path("round_trip_selectors.mrc");
		const auto values = counting(count_elements(subject.extents));

		auto source =
			make_host_array<float>(subject.extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data(),
			values.size() * sizeof(float)
		);

		const image_descriptor descriptor(
			make_span(subject.extents),
			subject.core_rank,
			numerical_type::float32
		);

		{
			const auto writer =
				writers.open(path.get(), descriptor, image_metadata());
			writer->write(
				const_array_ref(source), whole_of(subject.extents));
			writer->flush();
		}

		const auto reader = readers.open(path.get());

		REQUIRE( reader->get_descriptor() == descriptor );

		auto destination =
			make_host_array<float>(subject.extents, numerical_type::float32);
		reader->read(array_ref(destination), whole_of(subject.extents));

		REQUIRE( get_values<float>(destination) == values );
	}
}

TEST_CASE(
	"an MRC file converts to and from the type it holds",
	"[mrc][image_format_selector]"
)
{
	image_write_format_selector writers;
	writers.register_builtin_formats();
	image_read_format_selector readers;
	readers.register_builtin_formats();

	const std::vector<std::size_t> extents = {2, 2};
	const std::vector<float> values = {-2.0F, -1.0F, 0.0F, 300.0F};

	const std::array<numerical_type, 2> file_types = {
		numerical_type::int16,
		numerical_type::float32
	};

	for (const auto file_type : file_types)
	{
		const scoped_path path("round_trip_conversion.mrc");

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

		const auto read = get_values<double>(destination);

		REQUIRE( read == std::vector<double>(
			values.begin(), values.end()) );
	}
}
