// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_metadata.hpp"

#include "dm_tag_values.hpp"

#include <vitrio/image_metadata_field.hpp>
#include <vitrio/image_metadata_fields.hpp>

#include <cmath>
#include <cstddef>
#include <string>

namespace vitrio
{
namespace dm
{

namespace
{

// The fields that state the distance between the samples along the axes of
// an image, in the order the file calibrates them in: its columns first.
constexpr image_metadata_field<double> pixel_size_fields[] = {
	image_metadata_fields::pixel_size_x,
	image_metadata_fields::pixel_size_y
};

constexpr std::size_t pixel_size_field_count =
	sizeof(pixel_size_fields) / sizeof(pixel_size_fields[0]);

// How many ångströms a unit of length is, as DigitalMicrograph spells it,
// or zero for what is no length this format knows. The micro and the
// ångström each have two characters that look alike, and files have both.
double get_angstroms(const std::u16string &units) noexcept
{
	struct length
	{
		const char16_t *name;
		double angstroms;
	};

	static const length lengths[] = {
		{u"Å", 1.0},
		{u"Å", 1.0},
		{u"pm", 1.0e-2},
		{u"nm", 1.0e1},
		{u"µm", 1.0e4},
		{u"μm", 1.0e4},
		{u"um", 1.0e4},
		{u"mm", 1.0e7}
	};

	for (const auto &known : lengths)
	{
		if (units == known.name)
		{
			return known.angstroms;
		}
	}

	return 0.0;
}

const dm_tag* find_group(const dm_tag &parent, const char *label) noexcept
{
	const auto *child = parent.find_child(label);
	return child && child->is_group() ? child : nullptr;
}

// The distance between the samples along an axis, from the group that
// calibrates it, or zero where it states none.
double get_pixel_size(
	const dm_tag &calibration,
	span<const byte> file,
	byte_order order
)
{
	const auto *scale = calibration.find_child("Scale");
	const auto *units = calibration.find_child("Units");
	if (!scale || !units || !holds_real(*scale) || !holds_text(*units))
	{
		return 0.0;
	}

	const auto pixel_size =
		read_real(*scale, file, order) *
		get_angstroms(read_text(*units, file, order));

	return std::isfinite(pixel_size) && pixel_size > 0.0 ? pixel_size : 0.0;
}

} // anonymous namespace

image_metadata derive_metadata(
	const dm_tag &image,
	span<const byte> file,
	byte_order order
)
{
	image_metadata result;

	const auto *image_data = find_group(image, "ImageData");
	const auto *calibrations =
		image_data ? find_group(*image_data, "Calibrations") : nullptr;
	const auto *axes =
		calibrations ? find_group(*calibrations, "Dimension") : nullptr;
	if (!axes)
	{
		return result;
	}

	const auto count = axes->get_child_count();
	for (std::size_t i = 0; i < count && i < pixel_size_field_count; ++i)
	{
		const auto pixel_size = get_pixel_size(axes->get_child(i), file, order);
		if (pixel_size > 0.0)
		{
			set_field(result, pixel_size_fields[i], pixel_size);
		}
	}

	return result;
}

} // namespace dm
} // namespace vitrio
