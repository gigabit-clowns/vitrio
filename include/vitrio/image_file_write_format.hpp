// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_writer.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_descriptor;
class image_metadata;
class image_file_probe;

/**
 * @brief The ability of one image file format to be written.
 *
 * Judges from an @ref image_file_probe how well it fits a file, and creates a
 * file it fits, returning an @ref image_writer for it.
 *
 * @see image_file_read_format
 */
class VITRIO_API image_file_write_format
{
public:
	image_file_write_format() noexcept;
	image_file_write_format(const image_file_write_format &other) = delete;
	image_file_write_format(image_file_write_format &&other) = delete;
	virtual ~image_file_write_format();

	image_file_write_format&
	operator=(const image_file_write_format &other) = delete;
	image_file_write_format&
	operator=(image_file_write_format &&other) = delete;

	/**
	 * @brief Get the name of this format.
	 *
	 * @return std::string The name, which identifies the format.
	 */
	virtual std::string get_name() const = 0;

	/**
	 * @brief Report how well this format fits a file.
	 *
	 * The file named by @p probe need not exist, in which case the probe
	 * carries no leading bytes and the decision rests on the extension
	 * alone. Check @ref image_file_probe::exists rather than assuming there are
	 * leading bytes to read.
	 *
	 * @param probe The file under consideration.
	 * @return image_file_format_suitability How well this format fits @p probe.
	 */
	virtual image_file_format_suitability
	get_suitability(const image_file_probe &probe) const = 0;

	/**
	 * @brief Create a file and open it for writing.
	 *
	 * The descriptor is complete, so the file may be laid out in full before
	 * anything is written. Any file already at that path is replaced.
	 *
	 * How the file lays its samples out is the format's own decision, so the
	 * shape comes as an @ref image_descriptor, with no strides or offset.
	 *
	 * @param probe The file to create.
	 * @param descriptor What the file holds. Its data type is what the file
	 * stores rather than what a write will supply, since a write converts.
	 * @param metadata How its samples map onto physical space. A format
	 * writes what of it it can carry and ignores the rest.
	 * @return std::shared_ptr<image_writer> The opened writer, never null.
	 * @throws unsupported_operation_error If this format can not represent
	 * the requested file, such as a rank or a data type it has no encoding
	 * for.
	 * @throws image_file_error If the file could not be created.
	 */
	virtual std::shared_ptr<image_writer> open(
		const image_file_probe &probe,
		const image_descriptor &descriptor,
		const image_metadata &metadata
	) const = 0;
};

} // namespace vitrio
