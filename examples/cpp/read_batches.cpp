// SPDX-License-Identifier: LGPL-2.1-or-later

// Reads the images of a stack in batches, and starts reading the next batch
// before it uses the current one. A program that goes through many images
// should read them this way, and not one image per call. The example makes
// its own input and removes what it writes.

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/caching_image_reader_provider.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/executor_image_loader.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_loader.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

const std::size_t image_count = 64;
const std::size_t batch_size = 16;
const std::size_t row_count = 8;
const std::size_t column_count = 12;
const std::size_t image_size = row_count * column_count;

vitrio::array make_images(std::size_t count)
{
	const std::vector<std::size_t> extents = {
		count, row_count, column_count
	};
	return vitrio::make_array(
		vitrio::make_contiguous_array_descriptor(
			vitrio::make_span(extents),
			vitrio::numerical_type::float32
		)
	);
}

// Writes a stack whose image number i is filled with the value i.
void write_stack(
	const std::string &path,
	const vitrio::file_transfer_pool &transfers
)
{
	auto stack = make_images(image_count);
	auto *values = reinterpret_cast<float *>(stack.get_data());
	for (std::size_t i = 0; i < image_count * image_size; ++i)
	{
		values[i] = static_cast<float>(i / image_size);
	}

	vitrio::write_stack(
		stack,
		path,
		*vitrio::image_file_write_format_selector::get_shared(),
		transfers
	);
}

// A batch that is being read: its first image, the array it lands in and
// what to wait on.
struct pending_batch
{
	std::size_t first;
	vitrio::array images;
	std::shared_ptr<vitrio::completion> reading;
};

// Starts reading a batch and returns at once. The loader reads it on its
// threads, into an array of the batch's own.
pending_batch start_reading(
	const vitrio::image_loader &loader,
	const std::string &path,
	std::size_t first
)
{
	std::vector<vitrio::image_location> locations;
	for (std::size_t i = 0; i < batch_size; ++i)
	{
		locations.emplace_back(path, first + i);
	}
	const std::vector<vitrio::image_location> &batch = locations;

	auto images = make_images(batch_size);
	auto reading = vitrio::read_batch_async(
		loader, images.share(), vitrio::make_span(batch)
	);
	return {first, std::move(images), std::move(reading)};
}

// Waits for a batch and checks it, which stands in for the work a program
// does on its images.
bool check(const pending_batch &batch)
{
	batch.reading->get(); // Throws if a read failed.

	const auto *values =
		reinterpret_cast<const float *>(batch.images.get_data());
	for (std::size_t i = 0; i < batch_size * image_size; ++i)
	{
		if (values[i] != static_cast<float>(batch.first + i / image_size))
		{
			std::cerr << "Image " << batch.first + i / image_size
			          << " did not arrive.\n";
			return false;
		}
	}
	return true;
}

bool read_in_batches(
	const std::string &path,
	std::shared_ptr<const vitrio::file_transfer_pool> transfers
)
{
	// The loader opens files and starts their reads on a thread pool of its
	// own. It keeps the stack open between batches.
	const auto files = std::make_shared<vitrio::file_image_reader_provider>(
		vitrio::image_file_read_format_selector::get_shared(),
		std::move(transfers)
	);
	const vitrio::executor_image_loader loader(
		std::make_shared<vitrio::caching_image_reader_provider>(files, 1),
		std::make_shared<vitrio::thread_pool_executor>(2)
	);

	pending_batch current = start_reading(loader, path, 0);
	for (
		std::size_t first = batch_size;
		first < image_count;
		first += batch_size
	)
	{
		// The next batch is read in the background while this one is used.
		pending_batch upcoming = start_reading(loader, path, first);
		if (!check(current))
		{
			return false;
		}
		current = std::move(upcoming);
	}
	return check(current);
}

} // namespace

int main()
{
	const std::string path = "vitrio_example_read_batches.mrcs";

	// The threads that carry out the transfers of the files, which hand the
	// conversions to a thread pool.
	const auto transfers = std::make_shared<vitrio::file_transfer_pool>(
		std::make_shared<vitrio::thread_pool_executor>(2)
	);

	write_stack(path, *transfers);
	const bool succeeded = read_in_batches(path, transfers);
	std::remove(path.c_str());
	if (!succeeded)
	{
		return 1;
	}

	std::cout << "Read " << image_count << " images in batches of "
	          << batch_size << ".\n";
	return 0;
}
