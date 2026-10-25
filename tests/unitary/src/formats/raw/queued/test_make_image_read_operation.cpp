// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <formats/raw/queued/make_image_read_operation.hpp>

#include "../../../fixtures/file_transfer_executors.hpp"
#include "../../../mock/mock_file_transfer_scope.hpp"

#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
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
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <trompeloeil.hpp>

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

std::shared_ptr<const boost::interprocess::file_mapping>
open_file(const std::string &path)
{
	return std::make_shared<const boost::interprocess::file_mapping>(
		path.c_str(),
		boost::interprocess::read_only
	);
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

// What was asked of one read.
struct queued_read
{
	std::uint64_t offset;
	std::size_t buffer_count;
	std::size_t size;
};

// The reads an operation queues as it starts.
std::vector<queued_read> reads_started(file_transfer_operation &operation)
{
	std::vector<queued_read> reads;
	mock_file_transfer_scope scope;
	ALLOW_CALL(
		scope,
		queue_read(
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_,
			trompeloeil::_
		)
	)
		.LR_SIDE_EFFECT(
			std::size_t size = 0;
			for (const auto &buffer : _3)
			{
				size += buffer.get_size();
			}
			reads.push_back({_2, _3.size(), size})
		);
	FORBID_CALL(scope, run(trompeloeil::_, trompeloeil::_));

	operation.start(scope);
	return reads;
}

} // anonymous namespace

TEST_CASE(
	"make_image_read_operation reads regions that lie as they land straight "
	"into the array",
	"[make_image_read_operation]"
)
{
	const scoped_path path("make_image_read_operation_straight.raw");
	const auto size = write_file(path.get(), 16, counting(0, 36));
	const std::vector<std::size_t> images = {0, 1, 2, 4};

	SECTION( "the images that follow one another in the file are one read" )
	{
		const auto operation = make_image_read_operation(
			path.get(),
			open_file(path.get()),
			size,
			make_layout(stack_extents, 16),
			make_host_array<float>({4, 2, 3}, numerical_type::float32),
			images_of(images),
			any_budget
		);

		const auto reads = reads_started(*operation);
		REQUIRE( reads.size() == 2 );
		CHECK( reads[0].offset == 16 );
		CHECK( reads[0].buffer_count == 3 );
		CHECK( reads[0].size == 3 * image_size );
		CHECK( reads[1].offset == 16 + 4 * image_size );
		CHECK( reads[1].buffer_count == 1 );
	}

	SECTION( "and they land" )
	{
		const auto transfers =
			GENERATE(from_range(make_every_file_transfer_executor()));
		auto destination =
			make_host_array<float>({4, 2, 3}, numerical_type::float32, -1.0F);

		run_now(
			*transfers,
			make_image_read_operation(
				path.get(),
				open_file(path.get()),
				size,
				make_layout(stack_extents, 16),
				destination.share(),
				images_of(images),
				any_budget
			)
		);

		CHECK( get_values<float>(destination) ==
			values_of_images<float>(images) );
	}
}

TEST_CASE(
	"make_image_read_operation reads regions it converts into memory of "
	"their own, a step at a time",
	"[make_image_read_operation]"
)
{
	const scoped_path path("make_image_read_operation_steps.raw");
	const auto size = write_file(path.get(), 16, counting(0, 36));
	const std::vector<std::size_t> images = {0, 1, 2, 3, 4, 5};

	SECTION( "two images a step, each step one stretch, two steps ahead" )
	{
		const auto operation = make_image_read_operation(
			path.get(),
			open_file(path.get()),
			size,
			make_layout(stack_extents, 16),
			make_host_array<double>(stack_extents, numerical_type::float64),
			images_of(images),
			2 * image_size
		);

		const auto reads = reads_started(*operation);
		REQUIRE( reads.size() == 2 );
		for (std::size_t step = 0; step < reads.size(); ++step)
		{
			CHECK( reads[step].offset == 16 + 2 * step * image_size );
			CHECK( reads[step].size == 2 * image_size );
		}
	}

	SECTION( "and they land converted" )
	{
		const auto transfers =
			GENERATE(from_range(make_every_file_transfer_executor()));
		auto destination = make_host_array<double>(
			stack_extents,
			numerical_type::float64,
			-1.0
		);

		run_now(
			*transfers,
			make_image_read_operation(
				path.get(),
				open_file(path.get()),
				size,
				make_layout(stack_extents, 16),
				destination.share(),
				images_of(images),
				2 * image_size
			)
		);

		CHECK( get_values<double>(destination) ==
			values_of_images<double>(images) );
	}
}

TEST_CASE(
	"make_image_read_operation reads what a layout lays out",
	"[make_image_read_operation]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("make_image_read_operation_layout.raw");

	SECTION( "values that are not aligned for their data type" )
	{
		const auto size = write_file(path.get(), 18, counting(0, 36));
		const auto layout = make_layout(stack_extents, 18);

		SECTION( "straight into the array" )
		{
			auto destination =
				make_host_array<float>(stack_extents, numerical_type::float32);
			run_now(
				*transfers,
				make_image_read_operation(
					path.get(),
					open_file(path.get()),
					size,
					layout,
					destination.share(),
					whole_of(stack_extents),
					any_budget
				)
			);

			CHECK( get_values<float>(destination) == counting(0, 36) );
		}

		SECTION( "converted" )
		{
			auto destination =
				make_host_array<double>(stack_extents, numerical_type::float64);
			run_now(
				*transfers,
				make_image_read_operation(
					path.get(),
					open_file(path.get()),
					size,
					layout,
					destination.share(),
					whole_of(stack_extents),
					any_budget
				)
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

		image_transfer_plan regions(image_transfer_shape({2, 3}, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{1, 1}),
			make_span(std::vector<std::size_t>{0, 0})
		);
		auto destination =
			make_host_array<float>({2, 3}, numerical_type::float32);

		run_now(
			*transfers,
			make_image_read_operation(
				path.get(),
				open_file(path.get()),
				size,
				make_layout(extents, 16),
				destination.share(),
				regions,
				any_budget
			)
		);

		CHECK( get_values<float>(destination) ==
			std::vector<float>{6, 7, 8, 11, 12, 13} );
	}

	SECTION( "an empty plan, which reads nothing" )
	{
		const auto size = write_file(path.get(), 16, counting(0, 36));
		const auto operation = make_image_read_operation(
			path.get(),
			open_file(path.get()),
			size,
			make_layout(stack_extents, 16),
			make_host_array<float>(stack_extents, numerical_type::float32),
			images_of({}),
			any_budget
		);

		CHECK( reads_started(*operation).empty() );
	}
}

TEST_CASE(
	"make_image_read_operation refuses regions it can not read",
	"[make_image_read_operation]"
)
{
	const scoped_path path("make_image_read_operation_refused.raw");
	const auto size = write_file(path.get(), 16, counting(0, 36));

	SECTION( "an image the stack does not have" )
	{
		REQUIRE_THROWS_AS(
			make_image_read_operation(
				path.get(),
				open_file(path.get()),
				size,
				make_layout(stack_extents, 16),
				make_host_array<float>({1, 2, 3}, numerical_type::float32),
				images_of({6}),
				any_budget
			),
			std::out_of_range
		);
	}

	SECTION( "an array that is not initialized" )
	{
		REQUIRE_THROWS_AS(
			make_image_read_operation(
				path.get(),
				open_file(path.get()),
				size,
				make_layout(stack_extents, 16),
				array(),
				images_of({0}),
				any_budget
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"the operations of make_image_read_operation fail with what can not be "
	"read",
	"[make_image_read_operation]"
)
{
	const auto transfers =
		GENERATE(from_range(make_every_file_transfer_executor()));
	const scoped_path path("make_image_read_operation_fails.raw");

	// What the errors say is checked where they are made, by the tests of
	// file_read_plan_execution. Read here, it is freed by the thread that
	// carried out the read, which the thread sanitizer takes for a race: it
	// does not see how libstdc++ counts the references to an exception.
	SECTION( "a file that ends before a region" )
	{
		// The file held six images when it was measured, and holds three.
		write_file(path.get(), 16, counting(0, 18));
		const auto measured = 16 + 6 * image_size;
		const auto layout = make_layout(stack_extents, 16);

		SECTION( "read straight into the array" )
		{
			REQUIRE_THROWS_AS(
				run_now(
					*transfers,
					make_image_read_operation(
						path.get(),
						open_file(path.get()),
						measured,
						layout,
						make_host_array<float>(
							stack_extents,
							numerical_type::float32
						),
						whole_of(stack_extents),
						any_budget
					)
				),
				image_file_format_error
			);
		}

		SECTION( "read to be converted" )
		{
			REQUIRE_THROWS_AS(
				run_now(
					*transfers,
					make_image_read_operation(
						path.get(),
						open_file(path.get()),
						measured,
						layout,
						make_host_array<double>(
							stack_extents,
							numerical_type::float64
						),
						images_of({0, 1, 2, 3, 4, 5}),
						image_size
					)
				),
				image_file_format_error
			);
		}
	}

	SECTION( "a data type the array's can not be produced from" )
	{
		const std::vector<std::complex<float>> stored(36);
		const auto size = write_file(path.get(), 16, stored);

		REQUIRE_THROWS_AS(
			run_now(
				*transfers,
				make_image_read_operation(
					path.get(),
					open_file(path.get()),
					size,
					make_layout(
						stack_extents,
						16,
						numerical_type::complex_float32
					),
					make_host_array<float>(
						stack_extents,
						numerical_type::float32
					),
					images_of({0, 1, 2, 3, 4, 5}),
					2 * sizeof(std::complex<float>) * 6
				)
			),
			unsupported_operation_error
		);
	}
}
