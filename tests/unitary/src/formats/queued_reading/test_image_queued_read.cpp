// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/queued_reading/image_queued_read.hpp>

#include "../../fixtures/file_read_queue_pools.hpp"
#include "../../fixtures/no_file.hpp"
#include "../../fixtures/recording_file_read_queue.hpp"

#include <formats/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory/byte_order.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <complex>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// A stack of six images of two rows of three float32 values, which count up
// from zero through the whole stack.
const std::vector<std::size_t> stack_extents = {6, 2, 3};
const std::size_t image_size = 6 * sizeof(float);

constexpr std::size_t any_budget = std::size_t{1} << 30;

std::vector<float> counting(std::size_t first, std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(first + i);
	}

	return values;
}

// Write a preamble followed by the bytes of some values, and give the size
// of the file.
template <typename T>
std::size_t write_file(
	const std::string &path,
	std::size_t preamble,
	const std::vector<T> &values
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
	return raw.size();
}

raw_image_layout make_layout(
	const std::vector<std::size_t> &extents,
	std::size_t preamble,
	numerical_type data_type = numerical_type::float32
)
{
	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return raw_image_layout(
		image_descriptor(make_span(extents), 2, data_type),
		strides,
		preamble,
		get_system_byte_order()
	);
}

// The plan that reads images of the stack, the one at each index of
// @p images into the slot of the array at the same index.
image_transfer_plan images_of(const std::vector<std::size_t> &images)
{
	image_transfer_plan regions(image_transfer_shape({2, 3}, 3, 3));
	for (std::size_t slot = 0; slot < images.size(); ++slot)
	{
		regions.add(
			make_span(std::vector<std::size_t>{images[slot], 0, 0}),
			make_span(std::vector<std::size_t>{slot, 0, 0})
		);
	}

	return regions;
}

template <typename T>
std::vector<T> values_of_images(const std::vector<std::size_t> &images)
{
	std::vector<T> values;
	for (const auto image : images)
	{
		for (const auto value : counting(6 * image, 6))
		{
			values.push_back(static_cast<T>(value));
		}
	}

	return values;
}

// A file opened for reading, as a reader opens it.
class opened_file
{
public:
	explicit opened_file(const std::string &path)
		: m_file(path.c_str(), boost::interprocess::read_only)
	{
	}

	file_handle_t get() const noexcept
	{
		return m_file.get_mapping_handle().handle;
	}

private:
	boost::interprocess::file_mapping m_file;
};

} // anonymous namespace

TEST_CASE(
	"read_queued_regions reads regions that lie as they land straight into "
	"the array",
	"[image_queued_read]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());

	const scoped_path path("image_queued_read_straight.raw");
	const auto size = write_file(path.get(), 16, counting(0, 36));
	const opened_file file(path.get());

	const std::vector<std::size_t> images = {0, 1, 2, 4};
	auto destination =
		make_host_array<float>({4, 2, 3}, numerical_type::float32, -1.0F);

	read_queued_regions(
		path.get(),
		file.get(),
		size,
		make_layout(stack_extents, 16),
		array_ref(destination),
		images_of(images),
		queue,
		any_budget
	);

	CHECK( get_values<float>(destination) == values_of_images<float>(images) );
	CHECK( queue.get_outstanding() == 0 );

	// The images that follow one another in the file are one read.
	const auto &reads = queue.get_reads();
	REQUIRE( reads.size() == 2 );
	CHECK( reads[0].offset == 16 );
	CHECK( reads[0].buffer_count == 3 );
	CHECK( reads[0].size == 3 * image_size );
	CHECK( reads[1].offset == 16 + 4 * image_size );
	CHECK( reads[1].buffer_count == 1 );
}

TEST_CASE(
	"read_queued_regions reads regions it converts into memory of their own, "
	"a step at a time",
	"[image_queued_read]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());

	const scoped_path path("image_queued_read_steps.raw");
	const auto size = write_file(path.get(), 16, counting(0, 36));
	const opened_file file(path.get());

	const std::vector<std::size_t> images = {0, 1, 2, 3, 4, 5};
	auto destination =
		make_host_array<double>(stack_extents, numerical_type::float64, -1.0);

	read_queued_regions(
		path.get(),
		file.get(),
		size,
		make_layout(stack_extents, 16),
		array_ref(destination),
		images_of(images),
		queue,
		2 * image_size
	);

	CHECK( get_values<double>(destination) ==
		values_of_images<double>(images) );
	CHECK( queue.get_outstanding() == 0 );

	// Two images a step, each step one stretch.
	const auto &reads = queue.get_reads();
	REQUIRE( reads.size() == 3 );
	for (std::size_t step = 0; step < reads.size(); ++step)
	{
		CHECK( reads[step].offset == 16 + 2 * step * image_size );
		CHECK( reads[step].size == 2 * image_size );
	}
}

TEST_CASE(
	"read_queued_regions reads what a layout lays out",
	"[image_queued_read]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	auto &queue = pool->get_queue();

	const scoped_path path("image_queued_read_layout.raw");

	SECTION( "values that are not aligned for their data type" )
	{
		const auto size = write_file(path.get(), 18, counting(0, 36));
		const opened_file file(path.get());
		const auto layout = make_layout(stack_extents, 18);

		SECTION( "straight into the array" )
		{
			auto destination =
				make_host_array<float>(stack_extents, numerical_type::float32);
			read_queued_regions(
				path.get(),
				file.get(),
				size,
				layout,
				array_ref(destination),
				whole_of(stack_extents),
				queue,
				any_budget
			);

			CHECK( get_values<float>(destination) == counting(0, 36) );
		}

		SECTION( "converted" )
		{
			auto destination =
				make_host_array<double>(stack_extents, numerical_type::float64);
			read_queued_regions(
				path.get(),
				file.get(),
				size,
				layout,
				array_ref(destination),
				whole_of(stack_extents),
				queue,
				any_budget
			);

			const auto expected = counting(0, 36);
			CHECK( get_values<double>(destination) ==
				std::vector<double>(expected.begin(), expected.end()) );
		}
	}

	SECTION( "a patch of an image" )
	{
		const std::vector<std::size_t> extents = {4, 5};
		const auto size = write_file(path.get(), 16, counting(0, 20));
		const opened_file file(path.get());

		image_transfer_plan regions(image_transfer_shape({2, 3}, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 1}),
			make_span(std::vector<std::size_t>{0, 0})
		);
		auto destination =
			make_host_array<float>({2, 3}, numerical_type::float32);

		read_queued_regions(
			path.get(),
			file.get(),
			size,
			make_layout(extents, 16),
			array_ref(destination),
			regions,
			queue,
			any_budget
		);

		CHECK( get_values<float>(destination) ==
			std::vector<float>{6, 7, 8, 11, 12, 13} );
	}

	SECTION( "an empty plan, which reads nothing" )
	{
		const auto size = write_file(path.get(), 16, counting(0, 36));
		const opened_file file(path.get());
		recording_file_read_queue recording(queue);

		auto destination =
			make_host_array<float>(stack_extents, numerical_type::float32);
		read_queued_regions(
			path.get(),
			file.get(),
			size,
			make_layout(stack_extents, 16),
			array_ref(destination),
			images_of({}),
			recording,
			any_budget
		);

		CHECK( recording.get_reads().empty() );
	}
}

TEST_CASE(
	"read_queued_regions leaves no read outstanding when it throws",
	"[image_queued_read]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());

	const scoped_path path("image_queued_read_throws.raw");
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path.get() + ": ")
	);

	SECTION( "a file that ends before a region, which is malformed" )
	{
		// The file held six images when it was measured, and holds three.
		write_file(path.get(), 16, counting(0, 18));
		const auto measured = 16 + 6 * image_size;
		const opened_file file(path.get());
		const auto layout = make_layout(stack_extents, 16);

		SECTION( "read straight into the array" )
		{
			auto destination =
				make_host_array<float>(stack_extents, numerical_type::float32);
			REQUIRE_THROWS_MATCHES(
				read_queued_regions(
					path.get(),
					file.get(),
					measured,
					layout,
					array_ref(destination),
					whole_of(stack_extents),
					queue,
					any_budget
				),
				image_file_format_error,
				names_the_file
			);
		}

		SECTION( "read to be converted" )
		{
			auto destination =
				make_host_array<double>(stack_extents, numerical_type::float64);
			REQUIRE_THROWS_MATCHES(
				read_queued_regions(
					path.get(),
					file.get(),
					measured,
					layout,
					array_ref(destination),
					images_of({0, 1, 2, 3, 4, 5}),
					queue,
					image_size
				),
				image_file_format_error,
				names_the_file
			);
		}
	}

	SECTION( "a file the system fails to read" )
	{
		const auto size = write_file(path.get(), 16, counting(0, 36));
		auto destination =
			make_host_array<float>(stack_extents, numerical_type::float32);

		REQUIRE_THROWS_MATCHES(
			read_queued_regions(
				path.get(),
				no_file,
				size,
				make_layout(stack_extents, 16),
				array_ref(destination),
				images_of({0, 2, 4}),
				queue,
				any_budget
			),
			image_file_error,
			names_the_file
		);
	}

	SECTION( "a data type the array's can not be produced from" )
	{
		const std::vector<std::complex<float>> stored(36);
		const auto size = write_file(path.get(), 16, stored);
		const opened_file file(path.get());
		auto destination =
			make_host_array<float>(stack_extents, numerical_type::float32);

		REQUIRE_THROWS_AS(
			read_queued_regions(
				path.get(),
				file.get(),
				size,
				make_layout(stack_extents, 16, numerical_type::complex_float32),
				array_ref(destination),
				images_of({0, 1, 2, 3, 4, 5}),
				queue,
				2 * sizeof(std::complex<float>) * 6
			),
			unsupported_operation_error
		);
	}

	CHECK( queue.get_outstanding() == 0 );
}
