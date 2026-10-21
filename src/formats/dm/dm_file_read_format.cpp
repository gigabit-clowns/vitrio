// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_file_read_format.hpp"

#include "dm_extensions.hpp"
#include "dm_geometry.hpp"
#include "dm_header.hpp"
#include "dm_tag_parser.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_metadata.hpp>

#include <formats/image_file_format_registration_macros.hpp>
#include <formats/memory_mapping/image_file_access.hpp>
#include <formats/memory_mapping/image_file_layout.hpp>
#include <formats/memory_mapping/image_file_mapping.hpp>
#include <formats/open_raw_image_reader.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace dm
{

namespace
{

dm_image_location locate_file_image(const std::string &path)
{
	const image_file_mapping mapping(path, image_file_access::read_only);
	const auto file = make_span<const byte>(
		mapping.get_data(),
		mapping.get_size()
	);

	const auto header = parse_header(file);
	const auto root = parse_tags(file, header);

	return locate_image(root, file, header.get_byte_order());
}

std::vector<std::ptrdiff_t>
contiguous_strides(span<const std::size_t> extents)
{
	std::vector<std::ptrdiff_t> strides(extents.size());

	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return strides;
}

} // anonymous namespace

std::string dm_file_read_format::get_name() const
{
	return "DM";
}

image_file_format_suitability
dm_file_read_format::get_suitability(const image_file_probe &probe) const
{
	if (!has_dm_signature(probe.get_leading_bytes()))
	{
		return image_file_format_suitability::unsupported;
	}

	return is_readable_extension(probe.get_extension())
		? image_file_format_suitability::normal
		: image_file_format_suitability::fallback;
}

std::shared_ptr<image_reader>
dm_file_read_format::open(const image_file_probe &probe) const
{
	const auto location = locate_file_image(probe.get_path());
	const auto &descriptor = location.get_descriptor();

	// The extents fit in the values the tags hold, so their strides do not
	// overflow. DigitalMicrograph places the values wherever the tags before
	// them end, aligned or not, which the reader opened suits.
	return open_raw_image_reader(
		probe.get_path(),
		image_file_layout(
			descriptor,
			contiguous_strides(descriptor.get_extents()),
			location.get_data_offset(),
			location.get_byte_order()
		),
		image_metadata()
	);
}

VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(dm, vitrio::dm::dm_file_read_format);

} // namespace dm
} // namespace vitrio
