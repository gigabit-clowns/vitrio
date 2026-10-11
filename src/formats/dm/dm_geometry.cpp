// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_geometry.hpp"

#include "dm_data_type.hpp"
#include "dm_tag_values.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>

#include <array/checked_arithmetic.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace vitrio
{
namespace dm
{

namespace
{

// An image and a stack of images are both read a page at a time.
const std::size_t core_rank = 2;
const std::size_t min_rank = 2;
const std::size_t max_rank = 3;

const dm_tag& find_group(const dm_tag &parent, const char *label)
{
	const auto *child = parent.find_child(label);
	if (child == nullptr || !child->is_group())
	{
		throw image_file_format_error(
			std::string("dm::locate_image: The file has no group ") + label +
			"."
		);
	}

	return *child;
}

const dm_tag& find_datum(const dm_tag &parent, const char *label)
{
	const auto *child = parent.find_child(label);
	if (child == nullptr || child->is_group())
	{
		throw image_file_format_error(
			std::string("dm::locate_image: The image has no datum ") + label +
			"."
		);
	}

	return *child;
}

// The positions in the image list of the images that are thumbnails.
std::set<std::int64_t> find_thumbnails(
	const dm_tag &root,
	span<const byte> file,
	byte_order order
)
{
	std::set<std::int64_t> thumbnails;

	const auto *group = root.find_child("Thumbnails");
	if (group == nullptr || !group->is_group())
	{
		return thumbnails;
	}

	const auto count = group->get_child_count();
	for (std::size_t position = 0; position < count; ++position)
	{
		const auto &thumbnail = group->get_child(position);
		const auto *index = thumbnail.is_group()
			? thumbnail.find_child("ImageIndex")
			: nullptr;
		if (index != nullptr)
		{
			thumbnails.insert(read_integer(*index, file, order));
		}
	}

	return thumbnails;
}

const dm_tag& find_image(
	const dm_tag &root,
	span<const byte> file,
	byte_order order
)
{
	const auto &images = find_group(root, "ImageList");
	const auto thumbnails = find_thumbnails(root, file, order);

	const auto count = images.get_child_count();
	for (std::size_t position = 0; position < count; ++position)
	{
		const auto &image = images.get_child(position);
		const auto is_thumbnail =
			thumbnails.count(static_cast<std::int64_t>(position)) > 0;
		if (image.is_group() && !is_thumbnail)
		{
			return image;
		}
	}

	throw image_file_format_error(
		"dm::locate_image: The file holds no image but thumbnails."
	);
}

// The extents of the image, slowest axis first, from those the file states
// fastest axis first.
std::vector<std::size_t> read_extents(
	const dm_tag &dimensions,
	span<const byte> file,
	byte_order order
)
{
	const auto rank = dimensions.get_child_count();
	if (rank < min_rank || rank > max_rank)
	{
		throw unsupported_operation_error(
			"dm::locate_image: The image is of " + std::to_string(rank) +
			" axes, where only images of two and stacks of them of three "
			"are read."
		);
	}

	std::vector<std::size_t> extents(rank);
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		const auto extent = read_integer(
			dimensions.get_child(axis),
			file,
			order
		);
		if (extent < 0)
		{
			throw image_file_format_error(
				"dm::locate_image: The image states a negative extent."
			);
		}

		extents[rank - 1 - axis] = static_cast<std::size_t>(extent);
	}

	return extents;
}

// Whether the values of the image take the bytes of its datum, no more and
// no fewer.
bool fills_datum(
	const std::vector<std::size_t> &extents,
	numerical_type data_type,
	const dm_tag &data
)
{
	const auto limit = static_cast<std::size_t>(
		std::numeric_limits<std::ptrdiff_t>::max()
	);

	std::ptrdiff_t size = static_cast<std::ptrdiff_t>(get_size(data_type));
	for (const auto extent : extents)
	{
		if (extent > limit ||
			!checked_multiply(size, static_cast<std::ptrdiff_t>(extent), size))
		{
			return false;
		}
	}

	return static_cast<std::size_t>(size) == data.get_payload_size();
}

} // anonymous namespace

dm_image_location locate_image(
	const dm_tag &root,
	span<const byte> file,
	byte_order order
)
{
	const auto &image = find_image(root, file, order);
	const auto &image_data = find_group(image, "ImageData");

	const auto data_type = get_data_type(
		read_integer(find_datum(image_data, "DataType"), file, order)
	);
	const auto extents = read_extents(
		find_group(image_data, "Dimensions"),
		file,
		order
	);

	const auto &data = find_datum(image_data, "Data");
	if (!fills_datum(extents, data_type, data))
	{
		throw image_file_format_error(
			"dm::locate_image: The values of the image are not the size its "
			"extents and data type make."
		);
	}

	return dm_image_location(
		image_descriptor(make_span(extents), core_rank, data_type),
		data.get_payload_offset(),
		order
	);
}

} // namespace dm
} // namespace vitrio
