// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_metadata_field.hpp>
#include <vitrio/image_metadata_value_type.hpp>
#include <vitrio/span.hpp>

#include <cstdint>
#include <string>

namespace vitrio
{

/**
 * @brief The label and the type of one standard field, with the type as a
 * value rather than as the parameter of a template.
 *
 * It is made from the constant of @ref image_metadata_fields that states
 * both, so neither is stated a second time.
 */
class standard_image_metadata_field
{
public:
	/**
	 * @brief Construct from the field it describes.
	 *
	 * @param field The field.
	 */
	template <typename T>
	constexpr explicit standard_image_metadata_field(
		image_metadata_field<T> field
	) noexcept;

	constexpr const char* get_label() const noexcept;
	constexpr image_metadata_value_type get_type() const noexcept;

private:
	static constexpr image_metadata_value_type
	get_type_of(image_metadata_field<std::int64_t> field) noexcept;
	static constexpr image_metadata_value_type
	get_type_of(image_metadata_field<double> field) noexcept;
	static constexpr image_metadata_value_type
	get_type_of(image_metadata_field<std::string> field) noexcept;

	const char *m_label;
	image_metadata_value_type m_type;
};

/**
 * @brief Get the standard fields.
 *
 * @return span<const standard_image_metadata_field> Every field of
 * @ref image_metadata_fields, in no order to rely on.
 */
span<const standard_image_metadata_field>
get_standard_image_metadata_fields() noexcept;

/**
 * @brief Find the standard field a label names.
 *
 * @param label The label.
 * @return const standard_image_metadata_field* The field, or nullptr when
 * @p label names none.
 */
const standard_image_metadata_field*
find_standard_image_metadata_field(const std::string &label) noexcept;

} // namespace vitrio

#include "standard_image_metadata_field.inl"
