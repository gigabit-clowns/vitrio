// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_metadata_value_type.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace vitrio
{

/**
 * @brief One value of the metadata of an image file: an integer, a real or a
 * text.
 *
 * A value is of exactly one of the types of @c image_metadata_value_type,
 * settled when it is constructed. What it is constructed from decides which:
 * an integral type makes an integer, a floating point type a real, and a
 * string a text. A boolean and a pointer that is not a string make nothing,
 * and do not compile.
 *
 * @see image_metadata
 */
class image_metadata_value
{
public:
	/**
	 * @brief Construct an integer.
	 *
	 * @param value The number, of any integral type but @c bool.
	 * @throws std::out_of_range If @p value is above what a @c std::int64_t
	 * holds.
	 */
	template <
		typename T,
		typename std::enable_if<
			std::is_integral<T>::value && !std::is_same<T, bool>::value,
			int
		>::type = 0
	>
	image_metadata_value(T value);

	/**
	 * @brief Construct a real.
	 *
	 * @param value The number, of any floating point type.
	 */
	template <
		typename T,
		typename std::enable_if<
			std::is_floating_point<T>::value,
			int
		>::type = 0
	>
	image_metadata_value(T value) noexcept;

	/**
	 * @brief Construct a text.
	 *
	 * @param value The text.
	 */
	VITRIO_API
	image_metadata_value(std::string value) noexcept;

	/**
	 * @brief Construct a text.
	 *
	 * @param value The text, ended by a null character. Must not be null.
	 * @throws std::invalid_argument If @p value is null.
	 */
	VITRIO_API
	image_metadata_value(const char *value);

	image_metadata_value(bool value) = delete;
	image_metadata_value(std::nullptr_t value) = delete;

	VITRIO_API
	image_metadata_value(const image_metadata_value &other);
	VITRIO_API
	image_metadata_value(image_metadata_value &&other) noexcept;
	VITRIO_API
	~image_metadata_value();

	VITRIO_API
	image_metadata_value& operator=(const image_metadata_value &other);
	VITRIO_API
	image_metadata_value& operator=(image_metadata_value &&other) noexcept;

	/**
	 * @brief Get the type of this value.
	 *
	 * @return image_metadata_value_type The type.
	 */
	VITRIO_API
	image_metadata_value_type get_type() const noexcept;

	/**
	 * @brief Get this value as the integer it is.
	 *
	 * @return const std::int64_t& The integer. It refers to storage owned by
	 * this value.
	 * @throws std::invalid_argument If this value is not an integer.
	 */
	VITRIO_API
	const std::int64_t& get_integer() const;

	/**
	 * @brief Get this value as the real it is.
	 *
	 * An integer is not a real: it is not converted.
	 *
	 * @return const double& The real. It refers to storage owned by this
	 * value.
	 * @throws std::invalid_argument If this value is not a real.
	 */
	VITRIO_API
	const double& get_real() const;

	/**
	 * @brief Get this value as the text it is.
	 *
	 * @return const std::string& The text. It refers to storage owned by this
	 * value.
	 * @throws std::invalid_argument If this value is not a text.
	 */
	VITRIO_API
	const std::string& get_text() const;

	/**
	 * @brief Check whether two values are of the same type and hold the same.
	 *
	 * An integer never equals a real, whatever the two hold. Two reals
	 * compare as two @c double do.
	 */
	friend bool operator==(
		const image_metadata_value &lhs,
		const image_metadata_value &rhs
	) noexcept
	{
		// What a value does not hold is kept as it is constructed, so the
		// three compare whatever the type is.
		return
			lhs.m_type == rhs.m_type &&
			lhs.m_integer == rhs.m_integer &&
			lhs.m_real == rhs.m_real &&
			lhs.m_text == rhs.m_text;
	}

	friend bool operator!=(
		const image_metadata_value &lhs,
		const image_metadata_value &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	template <typename T>
	static std::int64_t to_integer(T value, std::true_type is_signed) noexcept;
	template <typename T>
	static std::int64_t to_integer(T value, std::false_type is_signed);

	VITRIO_API
	static std::int64_t to_integer(std::uintmax_t value);

	image_metadata_value_type m_type;
	std::int64_t m_integer;
	double m_real;
	std::string m_text;
};

} // namespace vitrio

#include "image_metadata_value.inl"
