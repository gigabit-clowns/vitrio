// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_metadata_value.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace vitrio
{

image_metadata_value::image_metadata_value(std::string value) noexcept
	: m_type(image_metadata_value_type::text)
	, m_integer(0)
	, m_real(0.0)
	, m_text(std::move(value))
{
}

image_metadata_value::image_metadata_value(const char *value)
	: m_type(image_metadata_value_type::text)
	, m_integer(0)
	, m_real(0.0)
	, m_text()
{
	if (!value)
	{
		throw std::invalid_argument(
			"image_metadata_value: The text must not be null."
		);
	}

	m_text = value;
}

image_metadata_value::image_metadata_value(
	const image_metadata_value &other
) = default;
image_metadata_value::image_metadata_value(
	image_metadata_value &&other
) noexcept = default;
image_metadata_value::~image_metadata_value() = default;

image_metadata_value&
image_metadata_value::operator=(const image_metadata_value &other) = default;
image_metadata_value& image_metadata_value::operator=(
	image_metadata_value &&other
) noexcept = default;

image_metadata_value_type image_metadata_value::get_type() const noexcept
{
	return m_type;
}

const std::int64_t& image_metadata_value::get_integer() const
{
	if (m_type != image_metadata_value_type::integer)
	{
		throw std::invalid_argument(
			"image_metadata_value::get_integer: The value is not an integer."
		);
	}

	return m_integer;
}

const double& image_metadata_value::get_real() const
{
	if (m_type != image_metadata_value_type::real)
	{
		throw std::invalid_argument(
			"image_metadata_value::get_real: The value is not a real."
		);
	}

	return m_real;
}

const std::string& image_metadata_value::get_text() const
{
	if (m_type != image_metadata_value_type::text)
	{
		throw std::invalid_argument(
			"image_metadata_value::get_text: The value is not a text."
		);
	}

	return m_text;
}

std::int64_t image_metadata_value::to_integer(std::uintmax_t value)
{
	const auto highest = static_cast<std::uintmax_t>(
		std::numeric_limits<std::int64_t>::max());
	if (value > highest)
	{
		throw std::out_of_range(
			"image_metadata_value: The integer is above what the metadata "
			"holds."
		);
	}

	return static_cast<std::int64_t>(value);
}

} // namespace vitrio
