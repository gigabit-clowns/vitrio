// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_metadata.hpp"

#include <vitrio/image_metadata_fields.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace vitrio
{
namespace tiff
{

namespace
{

constexpr double angstroms_per_centimetre = 1.0e8;

// A resolution is held as a quotient of two integers of 32 bits, so it
// goes no higher than the highest of those and no lower than one over it.
constexpr double max_resolution =
	static_cast<double>(std::numeric_limits<std::uint32_t>::max());
constexpr double min_resolution = 1.0 / max_resolution;

// Both ways: pixels to a centimetre from ångströms to a pixel, and back.
// Zero for what states nothing, and for what the other can not hold.
double invert(double value) noexcept
{
	if (!std::isfinite(value) || value <= 0.0)
	{
		return 0.0;
	}

	const auto inverse = angstroms_per_centimetre / value;
	if (!std::isfinite(inverse) || inverse <= 0.0)
	{
		return 0.0;
	}

	return inverse;
}

double to_resolution(double pixel_size) noexcept
{
	const auto resolution = invert(pixel_size);
	if (resolution < min_resolution || resolution > max_resolution)
	{
		return 0.0;
	}

	return resolution;
}

void set_pixel_size(
	image_metadata &metadata,
	image_metadata_field<double> field,
	double resolution
)
{
	const auto pixel_size = invert(resolution);
	if (pixel_size > 0.0)
	{
		set_field(metadata, field, pixel_size);
	}
}

void set_text(
	image_metadata &metadata,
	image_metadata_field<std::string> field,
	const std::string &text
)
{
	if (!text.empty())
	{
		set_field(metadata, field, text);
	}
}

// The text metadata states under a label of this format, or an empty one
// where it states none a tag holds: whoever states it may have stated
// anything under it, and a tag is a text that ends at a null character.
std::string find_text(
	const image_metadata &metadata,
	image_metadata_field<std::string> field
)
{
	const auto *value = metadata.find(field.get_label());
	if (!value || value->get_type() != image_metadata_value_type::text)
	{
		return std::string();
	}

	const auto &text = value->get_text();
	if (text.find('\0') != std::string::npos)
	{
		return std::string();
	}

	return text;
}

bool is_digit(char character) noexcept
{
	return character >= '0' && character <= '9';
}

// YYYY:MM:DD HH:MM:SS, of which only the form is checked.
bool is_date_time(const std::string &text) noexcept
{
	static const char form[] = "dddd:dd:dd dd:dd:dd";
	constexpr std::size_t length = sizeof(form) - 1;

	if (text.size() != length)
	{
		return false;
	}

	for (std::size_t i = 0; i < length; ++i)
	{
		const auto matches = form[i] == 'd'
			? is_digit(text[i])
			: text[i] == form[i];
		if (!matches)
		{
			return false;
		}
	}

	return true;
}

} // anonymous namespace

image_metadata derive_metadata(const tiff_page_tags &tags)
{
	image_metadata result;

	set_pixel_size(
		result, image_metadata_fields::pixel_size_x, tags.get_x_resolution());
	set_pixel_size(
		result, image_metadata_fields::pixel_size_y, tags.get_y_resolution());

	set_text(result, image_description_field, tags.get_image_description());
	set_text(result, software_field, tags.get_software());
	set_text(result, date_time_field, tags.get_date_time());

	return result;
}

tiff_page_tags make_page_tags(const image_metadata &metadata)
{
	tiff_page_tags result;

	const auto *pixel_size_x =
		find_field(metadata, image_metadata_fields::pixel_size_x);
	if (pixel_size_x)
	{
		result.set_x_resolution(to_resolution(*pixel_size_x));
	}

	const auto *pixel_size_y =
		find_field(metadata, image_metadata_fields::pixel_size_y);
	if (pixel_size_y)
	{
		result.set_y_resolution(to_resolution(*pixel_size_y));
	}

	result.set_image_description(find_text(metadata, image_description_field));
	result.set_software(find_text(metadata, software_field));

	auto date_time = find_text(metadata, date_time_field);
	if (is_date_time(date_time))
	{
		result.set_date_time(std::move(date_time));
	}

	return result;
}

} // namespace tiff
} // namespace vitrio
