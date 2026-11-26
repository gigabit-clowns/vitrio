// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_page_tags.hpp"

#include <utility>

namespace vitrio
{
namespace tiff
{

tiff_page_tags::tiff_page_tags() noexcept
	: m_x_resolution(0.0)
	, m_y_resolution(0.0)
	, m_image_description()
	, m_software()
	, m_date_time()
{
}

double tiff_page_tags::get_x_resolution() const noexcept
{
	return m_x_resolution;
}

void tiff_page_tags::set_x_resolution(double resolution) noexcept
{
	m_x_resolution = resolution;
}

double tiff_page_tags::get_y_resolution() const noexcept
{
	return m_y_resolution;
}

void tiff_page_tags::set_y_resolution(double resolution) noexcept
{
	m_y_resolution = resolution;
}

const std::string& tiff_page_tags::get_image_description() const noexcept
{
	return m_image_description;
}

void tiff_page_tags::set_image_description(std::string text) noexcept
{
	m_image_description = std::move(text);
}

const std::string& tiff_page_tags::get_software() const noexcept
{
	return m_software;
}

void tiff_page_tags::set_software(std::string text) noexcept
{
	m_software = std::move(text);
}

const std::string& tiff_page_tags::get_date_time() const noexcept
{
	return m_date_time;
}

void tiff_page_tags::set_date_time(std::string text) noexcept
{
	m_date_time = std::move(text);
}

} // namespace tiff
} // namespace vitrio
