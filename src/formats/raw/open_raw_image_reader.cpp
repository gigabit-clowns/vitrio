// SPDX-License-Identifier: LGPL-2.1-or-later

#include "open_raw_image_reader.hpp"

#include <formats/raw/mapped/mapped_image_reader.hpp>
#include <formats/raw/mapped/unaligned_mapped_image_reader.hpp>
#include <formats/raw/queued/queued_image_reader.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/file_transfer_pool.hpp>

#include <system/asynchronous_file_transfer_port_opener.hpp>
#include <file_transfer_pool_implementation.hpp>

#include <memory>
#include <utility>

namespace vitrio
{

std::shared_ptr<image_reader> open_raw_image_reader(
	const std::string &path,
	raw_image_layout layout,
	image_metadata metadata,
	const file_transfer_pool &transfers
)
{
	if (asynchronous_file_transfer_port_opener::is_supported())
	{
		return std::make_shared<queued_image_reader>(
			path,
			std::move(layout),
			std::move(metadata),
			transfers.get_implementation().get_executor()
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
