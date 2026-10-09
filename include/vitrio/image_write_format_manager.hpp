// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_writer.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_descriptor;
class image_metadata;
class image_probe;
class image_write_format;

/**
 * @brief Holds the image formats that can be written, and creates a file
 * with the most suitable of them.
 *
 * @ref register_builtin_formats adds the formats bundled with the library,
 * and @ref register_format adds any other.
 *
 * @see image_read_format_manager
 */
class VITRIO_API image_write_format_manager final
{
public:
	image_write_format_manager() noexcept;
	image_write_format_manager(
		const image_write_format_manager &other
	) = delete;
	image_write_format_manager(image_write_format_manager &&other) = delete;
	~image_write_format_manager();

	image_write_format_manager&
	operator=(const image_write_format_manager &other) = delete;
	image_write_format_manager&
	operator=(image_write_format_manager &&other) = delete;

	/**
	 * @brief Register the formats bundled with the library.
	 */
	void register_builtin_formats();

	/**
	 * @brief Register a new image write format.
	 *
	 * @param format The format to be registered.
	 * @return true The format was successfully registered.
	 * @return false The format was null.
	 */
	bool register_format(std::unique_ptr<image_write_format> format);

	/**
	 * @brief Create a file with the most suitable format.
	 *
	 * The file named by @p path need not exist, in which case the choice
	 * rests on its extension. Any file already there is replaced.
	 *
	 * @param path Path to the file to create.
	 * @param descriptor What the file holds.
	 * @param metadata How its samples map onto physical space.
	 * @return std::shared_ptr<image_writer> The opened writer, never null.
	 * @throws unsupported_operation_error If no registered format recognizes
	 * the file, or if the chosen one can not represent the requested file.
	 * @throws image_file_error If the file could not be created.
	 */
	std::shared_ptr<image_writer> open(
		const std::string &path,
		const image_descriptor &descriptor,
		const image_metadata &metadata
	) const;

	/**
	 * @brief Get the format that would create a file, without creating it.
	 *
	 * @param probe The file under consideration.
	 * @return const image_write_format* The most suitable format, or nullptr
	 * when none recognizes @p probe.
	 */
	const image_write_format*
	get_most_suitable_format(const image_probe &probe) const;

private:
	class implementation;
	VITRIO_STD_MEMBER_INTERFACE
	std::unique_ptr<implementation> m_implementation;

	implementation& create_if_null();
	const implementation& get_implementation() const noexcept;
};

} // namespace vitrio
