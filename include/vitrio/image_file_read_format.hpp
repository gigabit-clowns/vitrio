// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_reader.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_file_probe;

/**
 * @brief The ability of one image file format to be read.
 *
 * Judges from an @ref image_file_probe how well it fits a file, and opens a
 * file it fits as an @ref image_reader.
 *
 * @see image_file_write_format
 */
class VITRIO_API image_file_read_format
{
public:
	image_file_read_format() noexcept;
	image_file_read_format(const image_file_read_format &other) = delete;
	image_file_read_format(image_file_read_format &&other) = delete;
	virtual ~image_file_read_format();

	image_file_read_format&
	operator=(const image_file_read_format &other) = delete;
	image_file_read_format& operator=(image_file_read_format &&other) = delete;

	/**
	 * @brief Get the name of this format.
	 *
	 * @return std::string The name, which identifies the format.
	 */
	virtual std::string get_name() const = 0;

	/**
	 * @brief Report how well this format fits a file.
	 *
	 * Decide from @p probe alone, without opening the file: the probe
	 * carries its path, its lower case extension and its leading bytes.
	 *
	 * Return @ref image_file_format_suitability::unsupported for a file this
	 * format does not recognize, and something higher than
	 * @ref image_file_format_suitability::normal only to displace another
	 * format that recognizes the same file.
	 *
	 * @param probe The file under consideration.
	 * @return image_file_format_suitability How well this format fits @p probe.
	 */
	virtual image_file_format_suitability
	get_suitability(const image_file_probe &probe) const = 0;

	/**
	 * @brief Open a file for reading.
	 *
	 * @pre @ref get_suitability does not report @p probe as
	 * @ref image_file_format_suitability::unsupported.
	 *
	 * @param probe The file to open.
	 * @return std::shared_ptr<image_reader> The opened reader, never null.
	 * @throws image_file_error If the file can not be reached.
	 * @throws image_file_format_error If the file is malformed or truncated.
	 */
	virtual std::shared_ptr<image_reader> open(
		const image_file_probe &probe
	) const = 0;
};

} // namespace vitrio
