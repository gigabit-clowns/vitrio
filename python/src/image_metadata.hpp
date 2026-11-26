// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_metadata.hpp>

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Make the metadata a mapping given from Python states.
 *
 * Metadata crosses the binding as a mapping of labels to values and never
 * as a class of its own. A label is a string, and a value an integer, a
 * real or a text: an @c int, a @c float or a @c str, or what stands for a
 * number as the scalars of numpy do. A boolean is none of them, though
 * Python makes it an integer, and is refused.
 *
 * @param mapping The mapping, or None for metadata that states nothing.
 * @return image_metadata The metadata.
 * @throws nanobind::type_error If @p mapping is no mapping, if a label is
 * no string, or if a value is none of the three.
 * @throws nanobind::python_error If an integer is beyond what the metadata
 * holds, which Python reports as an @c OverflowError.
 * @throws std::invalid_argument If @ref image_metadata::set refuses a label
 * or the value given for it, which Python reports as a @c ValueError.
 */
image_metadata import_image_metadata(nanobind::handle mapping);

/**
 * @brief Make the dictionary Python is given for metadata.
 *
 * @param metadata The metadata.
 * @return nanobind::dict Its values under its labels, in the order of the
 * labels: an @c int for an integer, a @c float for a real and a @c str for
 * a text. A text is read as UTF-8, and what of it is not becomes the
 * replacement character.
 */
nanobind::dict export_image_metadata(const image_metadata &metadata);

} // namespace vitrio
