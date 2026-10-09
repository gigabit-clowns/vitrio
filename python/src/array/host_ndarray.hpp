// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/ndarray.h>

namespace vitrio
{

/**
 * @brief An array given from Python whose elements may be written.
 *
 * nanobind takes it from whatever hands out its memory through DLPack or
 * the buffer protocol, and copies nothing. It refuses memory that is on a
 * device and memory that is read-only.
 */
using host_ndarray = nanobind::ndarray<nanobind::device::cpu>;

/**
 * @brief An array given from Python whose elements are only read.
 *
 * As @ref host_ndarray, except that read-only memory is taken too.
 */
using const_host_ndarray =
	nanobind::ndarray<nanobind::ro, nanobind::device::cpu>;

} // namespace vitrio
