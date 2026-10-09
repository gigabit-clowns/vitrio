// SPDX-License-Identifier: LGPL-2.1-or-later

#include "array_memory.hpp"

#include "checked_arithmetic.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace vitrio
{

namespace
{

[[noreturn]]
void reject_unaddressable()
{
	throw std::out_of_range(
		"array: The elements span more memory than can be addressed."
	);
}

// A complex number is aligned as each of its two components is.
std::size_t get_alignment(numerical_type data_type) noexcept
{
	switch (data_type)
	{
	case numerical_type::complex_float16:
		return get_size(numerical_type::float16);
	case numerical_type::complex_float32:
		return get_size(numerical_type::float32);
	case numerical_type::complex_float64:
		return get_size(numerical_type::float64);
	default:
		return get_size(data_type);
	}
}

// Finds the lowest and the highest offset any element is at. Returns false
// when there is no element to be at one.
bool compute_offset_range(
	const array_descriptor &descriptor,
	std::ptrdiff_t &first,
	std::ptrdiff_t &last
)
{
	const auto extents = descriptor.get_extents();
	const auto strides = descriptor.get_strides();
	const auto highest = std::numeric_limits<std::ptrdiff_t>::max();

	first = descriptor.get_offset();
	last = descriptor.get_offset();
	for (std::size_t axis = 0; axis < extents.size(); ++axis)
	{
		if (extents[axis] == 0)
		{
			return false;
		}

		const auto steps = extents[axis] - 1;
		if (steps > static_cast<std::size_t>(highest))
		{
			reject_unaddressable();
		}

		// What the last element along this axis adds to the offset.
		std::ptrdiff_t reach = 0;
		const auto multiplied = checked_multiply(
			static_cast<std::ptrdiff_t>(steps),
			strides[axis],
			reach
		);
		if (!multiplied)
		{
			reject_unaddressable();
		}

		// A negative stride takes it below the origin instead of above.
		auto &bound = reach > 0 ? last : first;
		if (!checked_add(bound, reach, bound))
		{
			reject_unaddressable();
		}
	}

	return true;
}

} // anonymous namespace

std::size_t compute_storage_requirement(const array_descriptor &descriptor)
{
	std::ptrdiff_t first = 0;
	std::ptrdiff_t last = 0;
	if (!compute_offset_range(descriptor, first, last))
	{
		return 0;
	}

	if (first < 0)
	{
		throw std::out_of_range(
			"array: An element lies before the first byte of the memory."
		);
	}

	const auto size = get_size(descriptor.get_data_type());
	const auto count = static_cast<std::size_t>(last) + 1;
	if (count > std::numeric_limits<std::size_t>::max() / size)
	{
		reject_unaddressable();
	}

	return count * size;
}

void check_array_memory(
	span<const byte> memory,
	const array_descriptor &descriptor
)
{
	if (!is_initialized(descriptor))
	{
		throw std::invalid_argument(
			"array: The descriptor is not initialized."
		);
	}

	const auto address = reinterpret_cast<std::uintptr_t>(memory.data());
	if (address % get_alignment(descriptor.get_data_type()) != 0)
	{
		throw std::invalid_argument(
			"array: The memory is not aligned to its elements."
		);
	}

	if (compute_storage_requirement(descriptor) > memory.size())
	{
		throw std::out_of_range(
			"array: An element lies past the end of the memory."
		);
	}
}

} // namespace vitrio
