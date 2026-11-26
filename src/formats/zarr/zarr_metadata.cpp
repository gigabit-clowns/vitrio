// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_metadata.hpp"

#include <vitrio/image_metadata_field.hpp>
#include <vitrio/image_metadata_fields.hpp>

#include <assert.hpp>

#include <cmath>
#include <string>

namespace vitrio
{
namespace zarr
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

constexpr std::size_t pixel_size_field_count =
	sizeof(pixel_size_fields) / sizeof(pixel_size_fields[0]);

// How many angstroms a unit of length of OME-Zarr is, or zero for what is
// no length this format knows. The names are those of UDUNITS-2, as
// OME-Zarr has them.
double get_angstroms(const std::string &unit) noexcept
{
	struct length
	{
		const char *name;
		double angstroms;
	};

	static const length lengths[] = {
		{"angstrom", 1.0},
		{"picometer", 1.0e-2},
		{"nanometer", 1.0e1},
		{"micrometer", 1.0e4},
		{"millimeter", 1.0e7},
		{"centimeter", 1.0e8},
		{"meter", 1.0e10}
	};

	for (const auto &known : lengths)
	{
		if (unit == known.name)
		{
			return known.angstroms;
		}
	}

	return 0.0;
}

bool is_pixel_size(double value) noexcept
{
	return std::isfinite(value) && value > 0.0;
}

} // anonymous namespace

image_metadata derive_metadata(
	const zarr_multiscale &image,
	std::size_t level,
	const image_descriptor &descriptor
)
{
	VITRIO_ASSERT( level < image.datasets.size() );

	image_metadata result;

	const auto rank = descriptor.get_extents().size();
	const auto core_rank = descriptor.get_core_rank();
	const auto &scale = image.datasets[level].scale;
	if (image.axes.size() != rank || scale.size() != rank)
	{
		return result;
	}

	// The scale of every level counts only where it is one per axis.
	const auto has_shared_scale = image.scale.size() == rank;

	for (std::size_t i = 0; i < core_rank && i < pixel_size_field_count; ++i)
	{
		const auto axis = rank - 1 - i;
		if (image.axes[axis].type != "space")
		{
			continue;
		}

		const auto pixel_size =
			scale[axis] *
			(has_shared_scale ? image.scale[axis] : 1.0) *
			get_angstroms(image.axes[axis].unit);
		if (is_pixel_size(pixel_size))
		{
			set_field(result, pixel_size_fields[i], pixel_size);
		}
	}

	return result;
}

std::vector<double> derive_pixel_sizes(
	const image_metadata &metadata,
	const image_descriptor &descriptor
)
{
	const auto rank = descriptor.get_extents().size();
	const auto core_rank = descriptor.get_core_rank();

	std::vector<double> result(rank, 0.0);
	for (std::size_t i = 0; i < core_rank && i < pixel_size_field_count; ++i)
	{
		const auto *pixel_size = find_field(metadata, pixel_size_fields[i]);
		if (pixel_size && is_pixel_size(*pixel_size))
		{
			result[rank - 1 - i] = *pixel_size;
		}
	}

	return result;
}

} // namespace zarr
} // namespace vitrio
