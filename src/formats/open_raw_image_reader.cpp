// SPDX-License-Identifier: LGPL-2.1-or-later

#include "open_raw_image_reader.hpp"

#include <formats/memory_mapping/mapped_image_reader.hpp>
#include <formats/memory_mapping/unaligned_mapped_image_reader.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_reader.hpp>

#include <utility>

namespace vitrio
{

namespace
{

// What a reader of unaligned values can follow: an image or a stack of them,
// one value after another.
bool is_page_copyable(const image_file_layout &layout) noexcept
{
	const auto rank = layout.get_descriptor().get_extents().size();
	return layout.is_contiguous() && (rank == 2 || rank == 3);
}

} // anonymous namespace

std::shared_ptr<image_reader> open_raw_image_reader(
	const std::string &path,
	image_file_layout layout,
	image_metadata metadata
)
{
	if (layout.is_aligned())
	{
		return std::make_shared<mapped_image_reader>(
			path,
			std::move(layout),
			std::move(metadata)
		);
	}

	if (is_page_copyable(layout))
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
