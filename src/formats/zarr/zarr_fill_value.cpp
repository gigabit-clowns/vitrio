// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_fill_value.hpp"

#include "zarr_json.hpp"

#include <array/cast.hpp>
#include <array/numerical_type_dispatch.hpp>
#include <array/stored_boolean.hpp>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace vitrio
{
namespace zarr
{

namespace
{

template <typename T>
std::vector<byte> bytes_of(const T &value)
{
	std::vector<byte> bytes(sizeof(T));
	std::memcpy(bytes.data(), &value, sizeof(T));
	return bytes;
}

// The bits of a value written as "0x" and two hexadecimal digits a byte,
// most significant first, as version 3 writes them.
bool parse_hex_bits(const std::string &text, std::size_t size, byte *bits)
{
	if (text.size() != 2 + 2 * size || text[0] != '0' || text[1] != 'x')
	{
		return false;
	}

	std::uint64_t value = 0;
	for (std::size_t i = 2; i < text.size(); ++i)
	{
		const auto digit = text[i];
		std::uint64_t nibble;
		if (digit >= '0' && digit <= '9')
		{
			nibble = static_cast<std::uint64_t>(digit - '0');
		}
		else if (digit >= 'a' && digit <= 'f')
		{
			nibble = static_cast<std::uint64_t>(digit - 'a' + 10);
		}
		else if (digit >= 'A' && digit <= 'F')
		{
			nibble = static_cast<std::uint64_t>(digit - 'A' + 10);
		}
		else
		{
			return false;
		}
		value = value << 4 | nibble;
	}

	// The bits are those of a number of the size, in the order of the
	// system.
	switch (size)
	{
	case 2:
	{
		const auto narrow = static_cast<std::uint16_t>(value);
		std::memcpy(bits, &narrow, size);
		return true;
	}
	case 4:
	{
		const auto narrow = static_cast<std::uint32_t>(value);
		std::memcpy(bits, &narrow, size);
		return true;
	}
	case 8:
		std::memcpy(bits, &value, size);
		return true;
	default:
		return false;
	}
}

// One floating point value: a number, a name of what is not one, or bits.
template <typename T>
T parse_floating(const nlohmann::json &value, const std::string &where)
{
	if (value.is_number())
	{
		const auto number = value.get<double>();
		T result;
		cast(&result, &number);
		return result;
	}

	if (value.is_string())
	{
		const auto &text = value.get_ref<const std::string&>();
		double number;
		bool named = true;
		if (text == "NaN")
		{
			number = std::numeric_limits<double>::quiet_NaN();
		}
		else if (text == "Infinity")
		{
			number = std::numeric_limits<double>::infinity();
		}
		else if (text == "-Infinity")
		{
			number = -std::numeric_limits<double>::infinity();
		}
		else
		{
			named = false;
		}

		if (named)
		{
			T result;
			cast(&result, &number);
			return result;
		}

		T result;
		if (parse_hex_bits(
			text, sizeof(T), reinterpret_cast<byte*>(&result)))
		{
			return result;
		}
	}

	reject_metadata(where, "The fill value is no floating point number.");
}

template <typename T>
T parse_integer(const nlohmann::json &value, const std::string &where)
{
	if (value.is_number_unsigned())
	{
		const auto number = value.get<std::uint64_t>();
		if (number <= static_cast<std::uint64_t>(
			std::numeric_limits<T>::max()))
		{
			return static_cast<T>(number);
		}
	}
	else if (value.is_number_integer())
	{
		const auto number = value.get<std::int64_t>();
		if (
			number >= static_cast<std::int64_t>(
				std::numeric_limits<T>::min()) &&
			(number < 0 || static_cast<std::uint64_t>(number) <=
				static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
		)
		{
			return static_cast<T>(number);
		}
	}
	else if (value.is_number_float())
	{
		// Some writers state an integer as a floating point number, such
		// as 0.0. One that is whole and fits is taken.
		// The range is checked before the number is converted, which is
		// undefined for one out of range, as for a NaN. The maximum of a
		// type of 64 bits rounds up into a double, so the bound above is
		// one past it, which a double holds exactly.
		const auto number = value.get<double>();
		const auto end =
			static_cast<double>(std::numeric_limits<T>::max()) + 1.0;
		if (
			number >= static_cast<double>(std::numeric_limits<T>::min()) &&
			number < end &&
			std::trunc(number) == number
		)
		{
			return static_cast<T>(number);
		}
	}

	reject_metadata(
		where, "The fill value is no whole number the data type holds.");
}

template <typename T>
std::vector<byte> parse_element(
	const nlohmann::json &value,
	const std::string &where,
	std::true_type /* floating */
)
{
	return bytes_of(parse_floating<T>(value, where));
}

template <typename T>
std::vector<byte> parse_element(
	const nlohmann::json &value,
	const std::string &where,
	std::false_type /* integer */
)
{
	return bytes_of(parse_integer<T>(value, where));
}

template <typename T>
struct is_floating
	: std::integral_constant<
		bool,
		std::is_floating_point<T>::value ||
			std::is_same<T, float16_t>::value
	>
{
};

template <typename T>
std::vector<byte> parse_typed(
	const nlohmann::json &value,
	const std::string &where,
	type_tag<T>
)
{
	return parse_element<T>(value, where, is_floating<T>());
}

template <typename T>
std::vector<byte> parse_typed(
	const nlohmann::json &value,
	const std::string &where,
	type_tag<std::complex<T>>
)
{
	if (!value.is_array() || value.size() != 2)
	{
		reject_metadata(
			where, "The fill value of a complex type is no pair of parts.");
	}

	const std::complex<T> number(
		parse_floating<T>(value[0], where),
		parse_floating<T>(value[1], where)
	);
	return bytes_of(number);
}

std::vector<byte> parse_typed(
	const nlohmann::json &value,
	const std::string &where,
	type_tag<bool>
)
{
	stored_boolean result = {0};
	if (value.is_boolean())
	{
		result.value = value.get<bool>() ? 1 : 0;
	}
	else if (
		value.is_number_integer() &&
		(value.get<std::int64_t>() == 0 || value.get<std::int64_t>() == 1)
	)
	{
		result.value = static_cast<std::uint8_t>(value.get<std::int64_t>());
	}
	else
	{
		reject_metadata(where, "The fill value of a boolean is no boolean.");
	}

	return bytes_of(result);
}

} // anonymous namespace

std::vector<byte> parse_fill_value(
	const nlohmann::json &value,
	numerical_type type,
	const std::string &where
)
{
	if (value.is_null())
	{
		return std::vector<byte>(get_size(type), 0);
	}

	return dispatch_numerical_type(
		[&value, &where] (auto tag)
		{
			return parse_typed(value, where, tag);
		},
		type
	);
}

} // namespace zarr
} // namespace vitrio
