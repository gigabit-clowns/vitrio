// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <boost/optional.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief One axis of an image of OME-Zarr.
 */
struct zarr_axis
{
	/** @brief Its name, such as "z", "y" or "x". */
	std::string name;

	/** @brief Its type: "space", "time", "channel", another or none. */
	std::string type;

	/** @brief The unit of a step along it, such as "angstrom", or none. */
	std::string unit;
};

/**
 * @brief One level of resolution of an image of OME-Zarr: an array of the
 * group, and the size of a step of it along each axis.
 */
struct zarr_dataset
{
	/** @brief Where the array is, relative to the group. */
	std::string path;

	/** @brief The scale it states, one per axis, or none. */
	std::vector<double> scale;
};

/**
 * @brief An image of OME-Zarr: its axes and its levels of resolution, the
 * finest first.
 */
struct zarr_multiscale
{
	std::vector<zarr_axis> axes;
	std::vector<zarr_dataset> datasets;

	/**
	 * @brief The scale it states for every level, on top of the one of
	 * each, one per axis, or none.
	 */
	std::vector<double> scale;
};

/**
 * @brief Read the first image the attributes of a group state, as OME-Zarr
 * states them.
 *
 * Version 0.4 keeps the images under "multiscales", over ZARR version 2.
 * Version 0.5 keeps them under "ome", "multiscales", over version 3. An axis
 * may be stated by its name alone, as before 0.4, which gives "x", "y" and
 * "z" the type of space, "t" that of time and "c" that of a channel.
 *
 * Only a scale is read of the transformations, that of each level and the
 * one that applies to every level. A translation changes no scale.
 *
 * @param attributes The attributes of the group.
 * @param where What a message starts with.
 * @return boost::optional<zarr_multiscale> The image, or none when the
 * attributes state none.
 * @throws image_file_format_error If they state images that are not as
 * OME-Zarr states them, or none, or one of no levels.
 */
boost::optional<zarr_multiscale> parse_multiscale(
	const nlohmann::json &attributes,
	const std::string &where
);

} // namespace zarr
} // namespace vitrio
