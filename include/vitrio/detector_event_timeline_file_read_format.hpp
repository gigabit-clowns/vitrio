// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/export.hpp>
#include <vitrio/image_file_format_suitability.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_file_probe;

/**
 * @brief The ability of one file format of detector events to be read.
 *
 * Judges from an @ref image_file_probe how well it fits a file, and opens a
 * file it fits as a @ref detector_event_timeline_reader.
 *
 * @see image_file_read_format
 */
class VITRIO_API detector_event_timeline_file_read_format
{
public:
	detector_event_timeline_file_read_format() noexcept;
	detector_event_timeline_file_read_format(
		const detector_event_timeline_file_read_format &other
	) = delete;
	detector_event_timeline_file_read_format(
		detector_event_timeline_file_read_format &&other
	) = delete;
	virtual ~detector_event_timeline_file_read_format();

	detector_event_timeline_file_read_format& operator=(
		const detector_event_timeline_file_read_format &other
	) = delete;
	detector_event_timeline_file_read_format& operator=(
		detector_event_timeline_file_read_format &&other
	) = delete;

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
	 * Return @c image_file_format_suitability::unsupported for a file this
	 * format does not recognize, and something higher than
	 * @c image_file_format_suitability::normal only to displace another
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
	 * @c image_file_format_suitability::unsupported.
	 *
	 * @param probe The file to open.
	 * @return std::shared_ptr<detector_event_timeline_reader> The opened
	 * reader, never null.
	 * @throws image_file_error If the file can not be reached.
	 * @throws image_file_format_error If the file is malformed or truncated.
	 */
	virtual std::shared_ptr<detector_event_timeline_reader> open(
		const image_file_probe &probe
	) const = 0;
};

} // namespace vitrio
