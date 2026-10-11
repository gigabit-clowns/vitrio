// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_encoding.hpp"

#include "dm_constants.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <array/checked_arithmetic.hpp>

#include <limits>
#include <string>

namespace vitrio
{
namespace dm
{

namespace
{

// Walks the words of one encoding, each taken once.
class encoding_walk
{
public:
	explicit encoding_walk(span<const std::uint64_t> encoding) noexcept
		: m_encoding(encoding)
		, m_position(0)
	{
	}

	std::uint64_t take()
	{
		if (m_position == m_encoding.size())
		{
			throw image_file_format_error(
				"dm::get_encoded_size: The encoding of a value ends before "
				"the type it names does."
			);
		}

		return m_encoding[m_position++];
	}

	bool is_done() const noexcept
	{
		return m_position == m_encoding.size();
	}

private:
	span<const std::uint64_t> m_encoding;
	std::size_t m_position;
};

[[noreturn]] void throw_too_large()
{
	throw image_file_format_error(
		"dm::get_encoded_size: The encoding of a value states a size too "
		"large to address."
	);
}

std::ptrdiff_t to_offset(std::uint64_t value)
{
	if (value > static_cast<std::uint64_t>(
		std::numeric_limits<std::ptrdiff_t>::max()))
	{
		throw_too_large();
	}

	return static_cast<std::ptrdiff_t>(value);
}

std::ptrdiff_t add(std::ptrdiff_t lhs, std::ptrdiff_t rhs)
{
	std::ptrdiff_t result;
	if (!checked_add(lhs, rhs, result))
	{
		throw_too_large();
	}

	return result;
}

std::ptrdiff_t multiply(std::ptrdiff_t lhs, std::ptrdiff_t rhs)
{
	std::ptrdiff_t result;
	if (!checked_multiply(lhs, rhs, result))
	{
		throw_too_large();
	}

	return result;
}

std::ptrdiff_t get_scalar_size_of(std::uint64_t type)
{
	const auto size = get_scalar_size(type);
	if (size == 0)
	{
		throw image_file_format_error(
			"dm::get_encoded_size: The encoding of a value names a type "
			"the format does not have: " + std::to_string(type) + "."
		);
	}

	return static_cast<std::ptrdiff_t>(size);
}

std::ptrdiff_t take_size(encoding_walk &walk, std::size_t depth);

std::ptrdiff_t take_structure_size(encoding_walk &walk)
{
	walk.take(); // The length of its name.
	const auto field_count = walk.take();

	std::ptrdiff_t size = 0;
	for (std::uint64_t field = 0; field < field_count; ++field)
	{
		walk.take(); // The length of the name of the field.
		size = add(size, get_scalar_size_of(walk.take()));
	}

	return size;
}

std::ptrdiff_t take_array_size(encoding_walk &walk, std::size_t depth)
{
	const auto element_size = take_size(walk, depth + 1);
	const auto length = to_offset(walk.take());

	return multiply(element_size, length);
}

std::ptrdiff_t take_size(encoding_walk &walk, std::size_t depth)
{
	if (depth > max_encoding_depth)
	{
		throw image_file_format_error(
			"dm::get_encoded_size: The encoding of a value nests its types "
			"too deep."
		);
	}

	const auto type = walk.take();
	switch (type)
	{
	case encoding::structure:
		return take_structure_size(walk);

	case encoding::string:
		return to_offset(walk.take());

	case encoding::array:
		return take_array_size(walk, depth);

	default:
		return get_scalar_size_of(type);
	}
}

} // anonymous namespace

std::size_t get_scalar_size(std::uint64_t type) noexcept
{
	switch (type)
	{
	case encoding::boolean:
	case encoding::int8:
	case encoding::uint8:
		return 1;

	case encoding::int16:
	case encoding::uint16:
		return 2;

	case encoding::int32:
	case encoding::uint32:
	case encoding::float32:
		return 4;

	case encoding::int64:
	case encoding::uint64:
	case encoding::float64:
		return 8;

	default:
		return 0;
	}
}

bool is_integer(std::uint64_t type) noexcept
{
	switch (type)
	{
	case encoding::int8:
	case encoding::uint8:
	case encoding::int16:
	case encoding::uint16:
	case encoding::int32:
	case encoding::uint32:
	case encoding::int64:
	case encoding::uint64:
		return true;

	default:
		return false;
	}
}

std::size_t get_encoded_size(span<const std::uint64_t> encoding)
{
	encoding_walk walk(encoding);
	const auto size = take_size(walk, 1);

	if (!walk.is_done())
	{
		throw image_file_format_error(
			"dm::get_encoded_size: The encoding of a value holds words past "
			"the type it names."
		);
	}

	return static_cast<std::size_t>(size);
}

} // namespace dm
} // namespace vitrio
