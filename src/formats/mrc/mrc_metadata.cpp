// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mrc_metadata.hpp"

#include "mrc_constants.hpp"
#include "mrc_geometry.hpp"

#include <vitrio/image_metadata_fields.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace vitrio
{
namespace mrc
{

namespace
{

// The fields that state the distance between the samples along the axes of
// one image or volume, counted from the last of them.
constexpr image_metadata_field<double> pixel_size_fields[] = {
	image_metadata_fields::pixel_size_x,
	image_metadata_fields::pixel_size_y,
	image_metadata_fields::pixel_size_z
};

constexpr char label_separator = '\n';

// The sampling of the cell along an axis of space. The header names the
// three after the columns, the rows and the sections, but MRC2014 states
// them along X, Y and Z, whichever of those the columns run along.
std::int32_t
get_sampling(const mrc_header &header, std::int32_t space_axis) noexcept
{
	switch (space_axis)
	{
	case 1: return header.get_column_sampling();
	case 2: return header.get_row_sampling();
	default: return header.get_section_sampling();
	}
}

std::size_t to_cell_index(std::int32_t space_axis) noexcept
{
	return static_cast<std::size_t>(space_axis - 1);
}

// The distance between the samples along an axis of space, or zero where
// the header states none.
double
get_pixel_size(const mrc_header &header, std::int32_t space_axis) noexcept
{
	const auto sampling = get_sampling(header, space_axis);
	const auto size = static_cast<double>(
		header.get_cell_size()[to_cell_index(space_axis)]);

	if (sampling <= 0 || !std::isfinite(size) || size <= 0.0)
	{
		return 0.0;
	}

	return size / sampling;
}

void set_pixel_size(
	mrc_header &header,
	std::int32_t space_axis,
	double pixel_size
) noexcept
{
	if (!std::isfinite(pixel_size) || pixel_size <= 0.0)
	{
		return;
	}

	const auto size = pixel_size * get_sampling(header, space_axis);
	if (size > static_cast<double>(std::numeric_limits<float>::max()))
	{
		return;
	}

	// A cell that rounds to no size would state no distance at all.
	const auto stored = static_cast<float>(size);
	if (stored <= 0.0F)
	{
		return;
	}

	auto cell = header.get_cell_size();
	cell[to_cell_index(space_axis)] = stored;
	header.set_cell_size(cell);
}

std::string join_labels(span<const std::string> labels)
{
	std::string result;
	for (std::size_t i = 0; i < labels.size(); ++i)
	{
		if (i > 0)
		{
			result += label_separator;
		}

		result += labels[i];
	}

	return result;
}

std::vector<std::string> split_labels(const std::string &text)
{
	std::vector<std::string> result;

	std::size_t first = 0;
	for (;;)
	{
		const auto last = text.find(label_separator, first);
		if (last == std::string::npos)
		{
			result.push_back(text.substr(first));
			return result;
		}

		result.push_back(text.substr(first, last - first));
		first = last + 1;
	}
}

void add_labels(mrc_header &header, const std::string &text)
{
	for (const auto &label : split_labels(text))
	{
		if (header.get_labels().size() >= label_count)
		{
			return;
		}

		if (is_label(label))
		{
			header.add_label(label);
		}
	}
}

} // anonymous namespace

image_metadata derive_metadata(
	const mrc_header &header,
	mrc_single_section single_section
)
{
	image_metadata result;

	const auto space_axes = derive_core_space_axes(header, single_section);
	const auto core_rank = space_axes.size();
	for (std::size_t i = 0; i < core_rank; ++i)
	{
		const auto pixel_size =
			get_pixel_size(header, space_axes[core_rank - 1 - i]);
		if (pixel_size > 0.0)
		{
			set_field(result, pixel_size_fields[i], pixel_size);
		}
	}

	const auto labels = header.get_labels();
	if (!labels.empty())
	{
		set_field(result, labels_field, join_labels(labels));
	}

	return result;
}

void apply_metadata(
	const image_metadata &metadata,
	mrc_single_section single_section,
	mrc_header &header
)
{
	const auto space_axes = derive_core_space_axes(header, single_section);
	const auto core_rank = space_axes.size();
	for (std::size_t i = 0; i < core_rank; ++i)
	{
		const auto *pixel_size = find_field(metadata, pixel_size_fields[i]);
		if (pixel_size)
		{
			set_pixel_size(
				header, space_axes[core_rank - 1 - i], *pixel_size);
		}
	}

	// The label is the format's own and not a standard field, so whoever
	// states it may have stated anything under it.
	const auto *labels = metadata.find(labels_field.get_label());
	if (labels && labels->get_type() == image_metadata_value_type::text)
	{
		add_labels(header, labels->get_text());
	}
}

} // namespace mrc
} // namespace vitrio
