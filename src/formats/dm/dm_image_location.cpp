// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_image_location.hpp"

#include <utility>

namespace vitrio
{
namespace dm
{

dm_image_location::dm_image_location(
	image_descriptor descriptor,
	std::size_t data_offset,
	byte_order order
) noexcept
	: m_descriptor(std::move(descriptor))
	, m_data_offset(data_offset)
	, m_byte_order(order)
{
}

const image_descriptor& dm_image_location::get_descriptor() const noexcept
{
	return m_descriptor;
}

std::size_t dm_image_location::get_data_offset() const noexcept
{
	return m_data_offset;
}

byte_order dm_image_location::get_byte_order() const noexcept
{
	return m_byte_order;
}

} // namespace dm
} // namespace vitrio
