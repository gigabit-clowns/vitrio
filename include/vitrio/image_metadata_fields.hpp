// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_metadata_field.hpp>

namespace vitrio
{

/**
 * @brief The standard fields of @ref image_metadata.
 *
 * What these state means the same whatever the format of the file. The
 * axes they name are those of one image or volume, which are the trailing
 * extents of an @ref image_descriptor. Of those, @c x is the last, @c y the
 * one before it and @c z the one before that. An image has no @c z,
 * whether or not its file stacks several.
 */
namespace image_metadata_fields
{

/**
 * @brief The distance between two consecutive samples along @c x, in
 * ångströms. Finite and above zero.
 */
constexpr image_metadata_field<double> pixel_size_x{"pixel_size_x"};

/**
 * @brief The distance between two consecutive samples along @c y, in
 * ångströms. Finite and above zero.
 */
constexpr image_metadata_field<double> pixel_size_y{"pixel_size_y"};

/**
 * @brief The distance between two consecutive samples along @c z, in
 * ångströms. Finite and above zero. Only a volume states it.
 */
constexpr image_metadata_field<double> pixel_size_z{"pixel_size_z"};

} // namespace image_metadata_fields

} // namespace vitrio
