// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>

namespace vitrio
{

class array_ref;
class const_array_ref;

/**
 * @brief Make an array over the memory a reference names, which keeps
 * nothing alive.
 *
 * For a call that hands the array to a transfer and waits for it before
 * returning, so that the memory its caller keeps alive outlives the
 * transfer. The array, and every array sharing it, must not outlive the
 * memory.
 *
 * @param reference The reference. A reference to nothing makes an array
 * that holds nothing.
 * @return array An array over the same memory, with the same descriptor.
 */
array borrow_array(array_ref reference);

/**
 * @brief Make a read-only array over the memory a reference names, which
 * keeps nothing alive.
 *
 * As the other overload, read only.
 *
 * @param reference The reference. A reference to nothing makes an array
 * that holds nothing.
 * @return const_array A read-only array over the same memory, with the same
 * descriptor.
 */
const_array borrow_array(const_array_ref reference);

} // namespace vitrio
