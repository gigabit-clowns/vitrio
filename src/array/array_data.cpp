// SPDX-License-Identifier: LGPL-2.1-or-later

#include "array_data.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>

#include <stdexcept>

namespace vitrio
{

namespace
{

void check_initialized(const array_descriptor &descriptor)
{
	if (!is_initialized(descriptor))
	{
		throw std::invalid_argument("The array is not initialized.");
	}
}

} // anonymous namespace

byte* get_array_data(array_ref array)
{
	check_initialized(array.get_descriptor());
	return array.get_data();
}

const byte* get_array_data(const_array_ref array)
{
	check_initialized(array.get_descriptor());
	return array.get_data();
}

} // namespace vitrio
