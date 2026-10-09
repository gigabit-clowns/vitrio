// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>

#include <nanobind/ndarray.h>

namespace vitrio
{

/**
 * @brief Get the numerical type that a DLPack data type is.
 *
 * @param type The DLPack data type.
 * @return numerical_type The numerical type.
 * @throws nanobind::type_error If no numerical type holds such elements.
 */
numerical_type to_numerical_type(nanobind::dlpack::dtype type);

/**
 * @brief Get the DLPack data type that a numerical type is.
 *
 * @param type The numerical type. Must not be unknown.
 * @return nanobind::dlpack::dtype The DLPack data type.
 * @throws nanobind::type_error If @p type is unknown.
 */
nanobind::dlpack::dtype to_dlpack_data_type(numerical_type type);

} // namespace vitrio
