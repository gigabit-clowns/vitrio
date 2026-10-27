// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_scratch_storage_bounds.hpp"

#include <vitrio/array/array_descriptor.hpp>

#include <array/array_memory.hpp>

#include <stdexcept>
#include <string>

namespace vitrio
{

void check_bytes_within(
	std::size_t offset,
	std::size_t count,
	std::size_t size,
	const char *name
)
{
	if (offset > size || count > size - offset)
	{
		throw std::out_of_range(
			std::string(name) + ": The bytes reach past the storage."
		);
	}
}

void check_values_within(
	const array_descriptor &values,
	std::size_t size,
	const char *name
)
{
	if (!is_initialized(values))
	{
		throw std::invalid_argument(
			std::string(name) + ": The values are not described."
		);
	}

	if (compute_storage_requirement(values) > size)
	{
		throw std::out_of_range(
			std::string(name) + ": The values reach past the storage."
		);
	}
}

} // namespace vitrio
