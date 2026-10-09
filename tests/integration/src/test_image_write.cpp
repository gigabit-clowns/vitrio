// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_write.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/synchronous_executor.hpp>
#include <vitrio/executor_image_loader.hpp>
#include <vitrio/executor_image_saver.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/file_image_writer_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <cstddef>
#include <cstring>
#include <memory>
#include <numeric>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// A stack of six images of three by four, written and read back two at a
// time, so that neither end sees the stack whole.
const std::size_t stack_count = 6;
const std::size_t batch_size = 2;
const std::vector<std::size_t> stack_extents = {stack_count, 3, 4};
const std::vector<std::size_t> batch_extents = {batch_size, 3, 4};
const std::size_t core_rank = 2;

std::vector<float> counting(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(i) + 0.25F;
	}

	return values;
}

// The slots one batch covers, which is what a caller writing a stack a
// batch at a time names.
std::vector<image_location> slots_of(
	const std::string &path,
	std::size_t batch_index
)
{
	std::vector<image_location> slots;
	slots.reserve(batch_size);
	for (std::size_t i = 0; i < batch_size; ++i)
	{
		slots.emplace_back(path, batch_index * batch_size + i);
	}

	return slots;
}

} // anonymous namespace

TEST_CASE(
	"a stack written a batch at a time reads back a batch at a time",
	"[mrc][image_write]"
)
{
	const scoped_path path("batch_saver_stack.mrcs");
	const auto values = counting(count_elements(stack_extents));
	const auto batch_elements = count_elements(batch_extents);
	const auto batch_count = stack_count / batch_size;

	const auto writer_formats =
		image_file_write_format_selector::get_shared();
	const auto writers =
		std::make_shared<file_image_writer_provider>(writer_formats);
	const auto saver = std::make_shared<executor_image_saver>(
		writers,
		std::make_shared<synchronous_executor>()
	);

	// The size of the stack is settled here and nowhere else.
	const image_descriptor stack_descriptor(
		make_span(stack_extents),
		core_rank,
		numerical_type::float32
	);
	writers->declare(path.get(), stack_descriptor, image_metadata());

	std::vector<std::shared_ptr<completion>> written;
	for (std::size_t k = 0; k < batch_count; ++k)
	{
		auto source =
			make_host_array<float>(batch_extents, numerical_type::float32);
		std::memcpy(
			source.get_data(),
			values.data() + k * batch_elements,
			batch_elements * sizeof(float)
		);

		const auto slots = slots_of(path.get(), k);
		written.push_back(
			write_batch_async(*saver, source.share_const(), make_span(slots))
		);
	}

	for (const auto &completion : written)
	{
		REQUIRE_NOTHROW( completion->get() );
	}

	// Only once every batch has landed, which is what close's contract asks
	// of a caller.
	writers->close(path.get());

	const auto reader_formats =
		image_file_read_format_selector::get_shared();
	const auto readers =
		std::make_shared<file_image_reader_provider>(reader_formats);
	const auto loader = std::make_shared<executor_image_loader>(
		readers,
		std::make_shared<synchronous_executor>()
	);

	SECTION( "the file states the stack it was declared as" )
	{
		const auto reader = reader_formats->open(path.get());

		CHECK( reader->get_descriptor() == stack_descriptor );
	}

	SECTION( "every batch holds what was written into its slots" )
	{
		for (std::size_t k = 0; k < batch_count; ++k)
		{
			auto destination =
				make_host_array<float>(batch_extents, numerical_type::float32);

			const auto slots = slots_of(path.get(), k);
			const auto completion = read_batch_async(
				*loader,
				destination.share(),
				make_span(slots)
			);
			REQUIRE_NOTHROW( completion->get() );

			const std::vector<float> expected(
				values.begin() + k * batch_elements,
				values.begin() + (k + 1) * batch_elements
			);

			CHECK( get_values<float>(destination) == expected );
		}
	}
}

TEST_CASE(
	"a stack written whole reads back as a stack",
	"[mrc][image_write]"
)
{
	// The same extents would make one volume. write_stack is what makes the
	// file a stack of images instead.
	const scoped_path path("whole_stack.mrcs");
	const auto values = counting(count_elements(stack_extents));

	auto source =
		make_host_array<float>(stack_extents, numerical_type::float32);
	std::memcpy(
		source.get_data(),
		values.data(),
		values.size() * sizeof(float)
	);

	const auto writer_formats =
		image_file_write_format_selector::get_shared();
	write_stack(source, path.get(), *writer_formats);

	const auto reader_formats =
		image_file_read_format_selector::get_shared();
	file_image_reader_provider readers(reader_formats);

	SECTION( "the file states a stack of images of the array's type" )
	{
		const image_descriptor expected(
			make_span(stack_extents),
			core_rank,
			numerical_type::float32
		);

		CHECK( query_descriptor(readers, path.get()) == expected );
	}

	SECTION( "each image of the stack reads back on its own" )
	{
		const std::vector<std::size_t> image_extents = {3, 4};
		const auto image_elements = count_elements(image_extents);

		for (std::size_t k = 0; k < stack_count; ++k)
		{
			const auto image =
				vitrio::read(image_location(path.get(), k), readers);

			const auto extents = image.get_descriptor().get_extents();
			REQUIRE(
				std::vector<std::size_t>(extents.begin(), extents.end()) ==
				image_extents
			);

			const std::vector<float> expected(
				values.begin() + k * image_elements,
				values.begin() + (k + 1) * image_elements
			);

			CHECK( get_values<float>(image) == expected );
		}
	}

	SECTION( "an image reads back in the data type asked for" )
	{
		const std::vector<std::size_t> image_extents = {3, 4};
		const auto image_elements = count_elements(image_extents);

		const auto image = vitrio::read(
			image_location(path.get(), 1),
			readers,
			numerical_type::float64
		);

		REQUIRE(
			image.get_descriptor().get_data_type() ==
			numerical_type::float64
		);

		const std::vector<double> expected(
			values.begin() + image_elements,
			values.begin() + 2 * image_elements
		);

		CHECK( get_values<double>(image) == expected );
	}
}

TEST_CASE(
	"a stack of one image in a .mrcs file reads back as a stack of one",
	"[mrc][image_write]"
)
{
	// MRC2014 gives a stack of one image the header of a single image, and
	// the .mrcs name is what reads it back as a stack, as RELION does.
	const scoped_path path("stack_of_one.mrcs");
	const std::vector<std::size_t> one_extents = {1, 3, 4};
	const auto values = counting(count_elements(one_extents));
	const std::vector<image_location> slots = { image_location(path.get(), 0) };
	const image_descriptor descriptor(
		make_span(one_extents),
		core_rank,
		numerical_type::float32
	);

	const auto writer_formats =
		image_file_write_format_selector::get_shared();
	const auto writers =
		std::make_shared<file_image_writer_provider>(writer_formats);
	const executor_image_saver saver(
		writers,
		std::make_shared<synchronous_executor>()
	);
	writers->declare(path.get(), descriptor, image_metadata());

	auto source = make_host_array<float>(one_extents, numerical_type::float32);
	std::memcpy(
		source.get_data(),
		values.data(),
		values.size() * sizeof(float)
	);
	REQUIRE_NOTHROW(
		write_batch_async(saver, source.share_const(), make_span(slots))->get()
	);
	writers->close(path.get());

	const auto reader_formats =
		image_file_read_format_selector::get_shared();
	const auto readers =
		std::make_shared<file_image_reader_provider>(reader_formats);

	SECTION( "the file states the stack it was declared as" )
	{
		CHECK( query_descriptor(*readers, path.get()) == descriptor );
	}

	SECTION( "a batch reads the stack's only image back" )
	{
		const executor_image_loader loader(
			readers,
			std::make_shared<synchronous_executor>()
		);
		auto destination =
			make_host_array<float>(one_extents, numerical_type::float32);

		REQUIRE_NOTHROW(
			read_batch_async(
				loader,
				destination.share(),
				make_span(slots)
			)->get()
		);

		CHECK( get_values<float>(destination) == values );
	}
}
