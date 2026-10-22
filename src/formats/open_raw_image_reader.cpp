// SPDX-License-Identifier: LGPL-2.1-or-later

#include "open_raw_image_reader.hpp"

#include <formats/memory_mapping/mapped_image_reader.hpp>
#include <formats/memory_mapping/unaligned_mapped_image_reader.hpp>
#include <formats/queued_reading/queued_image_reader.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_reader.hpp>

#include <system/asynchronous_file_read_queue_pool.hpp>

#include <memory>
#include <utility>

namespace vitrio
{

namespace
{

// The queues every queued reader takes, made the first time one is opened.
const std::shared_ptr<file_read_queue_pool>& get_asynchronous_queue_pool()
{
	static const std::shared_ptr<file_read_queue_pool> queues =
		std::make_shared<asynchronous_file_read_queue_pool>();

	return queues;
}

} // anonymous namespace

std::shared_ptr<image_reader> open_raw_image_reader(
	const std::string &path,
	raw_image_layout layout,
	image_metadata metadata
)
{
	if (asynchronous_file_read_queue_pool::is_supported())
	{
		return std::make_shared<queued_image_reader>(
			path,
			std::move(layout),
			std::move(metadata),
			get_asynchronous_queue_pool()
		);
	}

	if (mapped_image_reader::supports(layout))
	{
		return std::make_shared<mapped_image_reader>(
			path,
			std::move(layout),
			std::move(metadata)
		);
	}

	if (unaligned_mapped_image_reader::supports(layout))
	{
		return std::make_shared<unaligned_mapped_image_reader>(
			path,
			layout.get_descriptor(),
			layout.get_data_offset(),
			layout.get_byte_order(),
			std::move(metadata)
		);
	}

	throw image_file_format_error(
		path + ": open_raw_image_reader: The values of the file are not "
		"aligned for their data type, and are not laid out one after another "
		"as an image or a stack of them."
	);
}

} // namespace vitrio
