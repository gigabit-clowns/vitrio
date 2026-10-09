// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "host_ndarray.hpp"
#include "memory_loan.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>

namespace vitrio
{

/**
 * @brief Make an array over the memory of one given from Python.
 *
 * Nothing is copied. The array does not keep the memory alive: the loan
 * does, until the array and every array sharing its memory are gone.
 *
 * @param source The array given from Python.
 * @param loan The loan of the memory of @p source.
 * @return array An array over the same elements.
 * @throws nanobind::type_error If no numerical type holds the elements of
 * @p source, or if what @p source came from hands out a buffer that cannot
 * be written to.
 * @throws std::invalid_argument If the memory of @p source is not aligned
 * to its elements, or if @p loan has been moved from.
 */
array import_array(const host_ndarray &source, const memory_loan &loan);

/**
 * @brief Make a read-only array over the memory of one given from Python.
 *
 * @param source The array given from Python.
 * @param loan The loan of the memory of @p source.
 * @return const_array An array over the same elements.
 * @throws nanobind::type_error If no numerical type holds the elements of
 * @p source.
 * @throws std::invalid_argument If the memory of @p source is not aligned
 * to its elements, or if @p loan has been moved from.
 *
 * @see import_array
 */
const_array import_const_array(
	const const_host_ndarray &source,
	const memory_loan &loan
);

} // namespace vitrio
