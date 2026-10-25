// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_multiscale.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Decide how many trailing axes of a ZARR array are one image or
 * volume, and so which lead it as a stack.
 *
 * ZARR does not say, so this is read from the first of these that states
 * it:
 * 1. "core_rank" under "vitrio" in the attributes of the array, which vitrio
 *    writes.
 * 2. The axes of OME-Zarr: the run of axes of space that ends the array, of
 *    three at most, is the core. A stack of images of two dimensions stored
 *    as three, as the tilt series of the CryoET Data Portal are, has all
 *    three of space, but the levels of lower resolution never scale the
 *    first: when they do scale another, the first is a stack.
 * 3. The names of the dimensions, of version 3: the run of names among z, y
 *    and x that ends them.
 * 4. Otherwise an array of three axes or fewer is one image or volume, and
 *    the trailing three of a larger one are.
 *
 * @param rank The rank of the array. Must not be zero.
 * @param attributes The attributes of the array.
 * @param dimension_names The names of its dimensions, none when unstated.
 * @param multiscale The image of OME-Zarr it is a level of, or null. Its axes
 * count only when there are as many as the array has.
 * @return std::size_t The core rank, from one to @p rank.
 */
std::size_t decide_core_rank(
	std::size_t rank,
	const nlohmann::json &attributes,
	const std::vector<std::string> &dimension_names,
	const zarr_multiscale *multiscale
);

} // namespace zarr
} // namespace vitrio
