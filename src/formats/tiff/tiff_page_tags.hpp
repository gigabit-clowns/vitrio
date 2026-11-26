// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace vitrio
{
namespace tiff
{

/**
 * @brief What a page of a TIFF file states of itself beyond how it holds
 * its samples.
 *
 * The resolution of a page is how many of its pixels there are to a
 * length, which the file states in a unit of its own: here it is always to
 * a centimetre. A resolution of zero is one the page does not state, and
 * so is a text that is empty.
 */
class tiff_page_tags
{
public:
	/**
	 * @brief Construct the tags of a page that states nothing.
	 */
	tiff_page_tags() noexcept;

	tiff_page_tags(const tiff_page_tags &other) = default;
	tiff_page_tags(tiff_page_tags &&other) noexcept = default;
	~tiff_page_tags() = default;

	tiff_page_tags& operator=(const tiff_page_tags &other) = default;
	tiff_page_tags& operator=(tiff_page_tags &&other) noexcept = default;

	/**
	 * @brief Get how many pixels there are to a centimetre along a row.
	 *
	 * @return double The resolution, or zero when the page states none.
	 */
	double get_x_resolution() const noexcept;
	void set_x_resolution(double resolution) noexcept;

	/**
	 * @brief Get how many pixels there are to a centimetre along a column.
	 *
	 * @return double The resolution, or zero when the page states none.
	 */
	double get_y_resolution() const noexcept;
	void set_y_resolution(double resolution) noexcept;

	/**
	 * @brief Get what the page says it shows, its ImageDescription.
	 *
	 * @return const std::string& The text, empty when the page states none.
	 */
	const std::string& get_image_description() const noexcept;
	void set_image_description(std::string text) noexcept;

	/**
	 * @brief Get the software the page says made it, its Software.
	 *
	 * @return const std::string& The text, empty when the page states none.
	 */
	const std::string& get_software() const noexcept;
	void set_software(std::string text) noexcept;

	/**
	 * @brief Get when the page says it was made, its DateTime.
	 *
	 * @return const std::string& The text, which the format has as
	 * <tt>YYYY:MM:DD HH:MM:SS</tt>. Empty when the page states none.
	 */
	const std::string& get_date_time() const noexcept;
	void set_date_time(std::string text) noexcept;

private:
	double m_x_resolution;
	double m_y_resolution;
	std::string m_image_description;
	std::string m_software;
	std::string m_date_time;
};

} // namespace tiff
} // namespace vitrio
