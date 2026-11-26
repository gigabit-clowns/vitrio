// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_metadata_field.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

template <typename T>
template <std::size_t N>
constexpr image_metadata_field<T>::image_metadata_field(
	const char (&label)[N]
) noexcept
	: m_label(label)
{
}

template <typename T>
constexpr const char* image_metadata_field<T>::get_label() const noexcept
{
	return m_label;
}

template <
	typename T,
	typename std::enable_if<
		std::is_arithmetic<T>::value && !std::is_same<T, bool>::value,
		int
	>::type
>
inline void set_field(
	image_metadata &metadata,
	image_metadata_field<double> field,
	T value
)
{
	// Converted here, so that an integer is held as a real under a label
	// the metadata does not know the type of.
	metadata.set(
		field.get_label(),
		image_metadata_value(static_cast<double>(value))
	);
}

template <
	typename T,
	typename std::enable_if<
		std::is_integral<T>::value && !std::is_same<T, bool>::value,
		int
	>::type
>
inline void set_field(
	image_metadata &metadata,
	image_metadata_field<std::int64_t> field,
	T value
)
{
	metadata.set(field.get_label(), image_metadata_value(value));
}

inline void set_field(
	image_metadata &metadata,
	image_metadata_field<std::string> field,
	std::string value
)
{
	metadata.set(field.get_label(), image_metadata_value(std::move(value)));
}

// A field is of one of three types, each of which finds its value through
// the accessor of that type.
template <>
inline const std::int64_t* find_field(
	const image_metadata &metadata,
	image_metadata_field<std::int64_t> field
)
{
	const auto *value = metadata.find(field.get_label());
	return value ? &value->get_integer() : nullptr;
}

template <>
inline const double* find_field(
	const image_metadata &metadata,
	image_metadata_field<double> field
)
{
	const auto *value = metadata.find(field.get_label());
	return value ? &value->get_real() : nullptr;
}

template <>
inline const std::string* find_field(
	const image_metadata &metadata,
	image_metadata_field<std::string> field
)
{
	const auto *value = metadata.find(field.get_label());
	return value ? &value->get_text() : nullptr;
}

template <typename T>
inline const T& get_field(
	const image_metadata &metadata,
	image_metadata_field<T> field
)
{
	const auto *value = find_field(metadata, field);
	if (!value)
	{
		throw std::out_of_range(
			std::string("get_field: The metadata states nothing under ") +
			field.get_label() + "."
		);
	}

	return *value;
}

} // namespace vitrio
