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
 * @brief Holds the image formats that can be written, selects the most suitable
 * of them for a file, and creates the file with it.
 *
 * @ref register_builtin_formats adds the formats bundled with the library,
 * and @ref register_format adds any other. @ref get_shared is a selector
 * that already holds the bundled ones.
 *
 * @par Thread safety
 * Every method may be called concurrently.
 *
 * @see image_read_format_selector
 */
class VITRIO_API image_write_format_selector final
{
public:
	/**
	 * @brief Construct a selector with no formats.
	 */
	image_write_format_selector();
	image_write_format_selector(
		const image_write_format_selector &other
	) = delete;
	image_write_format_selector(image_write_format_selector &&other) = delete;
	~image_write_format_selector();

	image_write_format_selector&
	operator=(const image_write_format_selector &other) = delete;
	image_write_format_selector&
	operator=(image_write_format_selector &&other) = delete;

	/**
	 * @brief Get the selector every use may share.
	 *
	 * It holds the formats bundled with the library. A format registered on
	 * it is seen by everyone that shares it.
	 *
	 * @return const std::shared_ptr<image_write_format_selector>& The selector.
	 * Never null.
	 */
	static const std::shared_ptr<image_write_format_selector>& get_shared();

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
};

} // namespace vitrio
