// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_metadata.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace vitrio
{

/**
 * @brief A label of @ref image_metadata together with the type of the value
 * under it.
 *
 * Stating the type with the label, once, is what lets a value be set and
 * got as that type and no other, through @c set_field, @c find_field and
 * @c get_field. The standard fields are the constants of
 * @ref image_metadata_fields.
 *
 * @tparam T The type of the value: @c std::int64_t for an integer,
 * @c double for a real or @c std::string for a text.
 */
template <typename T>
class image_metadata_field
{
	static_assert(
		std::is_same<T, std::int64_t>::value ||
		std::is_same<T, double>::value ||
		std::is_same<T, std::string>::value,
		"The value of a field is a std::int64_t, a double or a std::string."
	);

public:
	using value_type = T;

	/**
	 * @brief Construct a field from its label.
	 *
	 * @param label The label, as a string literal: the field refers to it
	 * and does not copy it.
	 */
	template <std::size_t N>
	constexpr explicit image_metadata_field(const char (&label)[N]) noexcept;

	/**
	 * @brief Get the label of this field.
	 *
	 * @return const char* The label, ended by a null character. Never null.
	 */
	constexpr const char* get_label() const noexcept;

private:
	const char *m_label;
};

/**
 * @brief State the value of a field that is a real.
 *
 * Replaces the value already under the label of the field, if any.
 *
 * @param metadata The metadata to state it in.
 * @param field The field.
 * @param value The value, of any arithmetic type but @c bool.
 * @throws std::invalid_argument If @ref image_metadata::set refuses the
 * label of @p field.
 */
template <
	typename T,
	typename std::enable_if<
		std::is_arithmetic<T>::value && !std::is_same<T, bool>::value,
		int
	>::type = 0
>
void set_field(
	image_metadata &metadata,
	image_metadata_field<double> field,
	T value
);

/**
 * @brief State the value of a field that is an integer.
 *
 * Replaces the value already under the label of the field, if any.
 *
 * @param metadata The metadata to state it in.
 * @param field The field.
 * @param value The value, of any integral type but @c bool.
 * @throws std::out_of_range If @p value is above what a @c std::int64_t
 * holds.
 * @throws std::invalid_argument If @ref image_metadata::set refuses the
 * label of @p field.
 */
template <
	typename T,
	typename std::enable_if<
		std::is_integral<T>::value && !std::is_same<T, bool>::value,
		int
	>::type = 0
>
void set_field(
	image_metadata &metadata,
	image_metadata_field<std::int64_t> field,
	T value
);

/**
 * @brief State the value of a field that is a text.
 *
 * Replaces the value already under the label of the field, if any.
 *
 * @param metadata The metadata to state it in.
 * @param field The field.
 * @param value The value.
 * @throws std::invalid_argument If @ref image_metadata::set refuses the
 * label of @p field.
 */
void set_field(
	image_metadata &metadata,
	image_metadata_field<std::string> field,
	std::string value
);

void set_field(
	image_metadata &metadata,
	image_metadata_field<std::string> field,
	std::nullptr_t value
) = delete;

/**
 * @brief Find the value of a field.
 *
 * @param metadata The metadata to look into.
 * @param field The field.
 * @return const T* The value, or nullptr when @p metadata states nothing
 * under the label of @p field. It refers to storage owned by @p metadata,
 * and is valid until the metadata is next modified.
 * @throws std::invalid_argument If the value under the label of @p field is
 * not of the type of the field, which that of a standard field always is.
 */
template <typename T>
const T* find_field(
	const image_metadata &metadata,
	image_metadata_field<T> field
);

/**
 * @brief Get the value of a field.
 *
 * @param metadata The metadata to look into.
 * @param field The field.
 * @return const T& The value. It refers to storage owned by @p metadata,
 * and is valid until the metadata is next modified.
 * @throws std::out_of_range If @p metadata states nothing under the label
 * of @p field.
 * @throws std::invalid_argument If the value under the label of @p field is
 * not of the type of the field, which that of a standard field always is.
 */
template <typename T>
const T& get_field(
	const image_metadata &metadata,
	image_metadata_field<T> field
);

} // namespace vitrio

#include "image_metadata_field.inl"
