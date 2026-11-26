// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "standard_image_metadata_field.hpp"

namespace vitrio
{

template <typename T>
constexpr standard_image_metadata_field::standard_image_metadata_field(
	image_metadata_field<T> field
) noexcept
	: m_label(field.get_label())
	, m_type(get_type_of(field))
{
}

constexpr const char*
standard_image_metadata_field::get_label() const noexcept
{
	return m_label;
}

constexpr image_metadata_value_type
standard_image_metadata_field::get_type() const noexcept
{
	return m_type;
}

constexpr image_metadata_value_type standard_image_metadata_field::get_type_of(
	image_metadata_field<std::int64_t> /*field*/
) noexcept
{
	return image_metadata_value_type::integer;
}

constexpr image_metadata_value_type standard_image_metadata_field::get_type_of(
	image_metadata_field<double> /*field*/
) noexcept
{
	return image_metadata_value_type::real;
}

constexpr image_metadata_value_type standard_image_metadata_field::get_type_of(
	image_metadata_field<std::string> /*field*/
) noexcept
{
	return image_metadata_value_type::text;
}

} // namespace vitrio
