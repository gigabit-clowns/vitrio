// SPDX-License-Identifier: LGPL-2.1-or-later

#include "standard_image_metadata_field.hpp"

#include <vitrio/image_metadata_fields.hpp>

#include <cstddef>

namespace vitrio
{

namespace
{

// Every constant of image_metadata_fields is named here, and here only. One
// left out is no standard field to image_metadata, which then refuses its
// label for having no dot.
constexpr standard_image_metadata_field standard_fields[] = {
	standard_image_metadata_field(image_metadata_fields::pixel_size_x),
	standard_image_metadata_field(image_metadata_fields::pixel_size_y),
	standard_image_metadata_field(image_metadata_fields::pixel_size_z)
};

constexpr std::size_t standard_field_count =
	sizeof(standard_fields) / sizeof(standard_fields[0]);

} // anonymous namespace

span<const standard_image_metadata_field>
get_standard_image_metadata_fields() noexcept
{
	return make_span(standard_fields, standard_field_count);
}

const standard_image_metadata_field*
find_standard_image_metadata_field(const std::string &label) noexcept
{
	// The fields are few, and a label is compared with each.
	for (const auto &field : standard_fields)
	{
		if (label == field.get_label())
		{
			return &field;
		}
	}

	return nullptr;
}

} // namespace vitrio
