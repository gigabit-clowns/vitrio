// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_probe;
class image_read_format;

/**
 * @brief Holds the image formats that can be read, selects the most suitable
 * of them for a file, and opens the file with it.
 *
 * @ref register_builtin_formats adds the formats bundled with the library,
 * and @ref register_format adds any other. @ref get_shared is a selector
 * that already holds the bundled ones.
 *
 * @par Thread safety
 * Every method may be called concurrently.
 *
 * @see image_write_format_selector
 */
class VITRIO_API image_read_format_selector final
{
public:
	/**
	 * @brief Construct a selector with no formats.
	 */
	image_read_format_selector();
	image_read_format_selector(
		const image_read_format_selector &other
	) = delete;
	image_read_format_selector(image_read_format_selector &&other) = delete;
	~image_read_format_selector();

	image_read_format_selector&
	operator=(const image_read_format_selector &other) = delete;
	image_read_format_selector&
	operator=(image_read_format_selector &&other) = delete;

	/**
	 * @brief Get the selector every use may share.
	 *
	 * It holds the formats bundled with the library. A format registered on
	 * it is seen by everyone that shares it.
	 *
	 * @return const std::shared_ptr<image_read_format_selector>& The selector.
	 * Never null.
	 */
	static const std::shared_ptr<image_read_format_selector>& get_shared();

	/**
	 * @brief Register the formats bundled with the library.
	 */
	void register_builtin_formats();

	/**
	 * @brief Register a new image read format.
	 *
	 * @param format The format to be registered.
	 * @return true The format was successfully registered.
	 * @return false The format was null.
	 */
	bool register_format(std::unique_ptr<image_read_format> format);

	/**
	 * @brief Open a file for reading with the most suitable format.
	 *
	 * Reads the head of the file once and shows it to every registered
	 * format, then opens the file with whichever reported the highest
	 * suitability.
	 *
	 * @param path Path to the file to open, or any other locator a
	 * registered format claims, such as the address of a remote file.
	 * @return std::shared_ptr<image_reader> The opened reader, never null.
	 * @throws image_file_error If no registered format claims the path and
	 * no readable file exists there.
	 * @throws unsupported_operation_error If no registered format recognizes
	 * the file, which exists.
	 * @throws image_format_error If the file is malformed or truncated.
	 */
	std::shared_ptr<image_reader> open(const std::string &path) const;

	/**
	 * @brief Get the format that would open a file, without opening it.
	 *
	 * Answers which format claims a file, and whether any does at all,
	 * without the cost or the failure modes of opening it.
	 *
	 * @param probe The file under consideration.
	 * @return const image_read_format* The most suitable format, or nullptr
	 * when none recognizes @p probe.
	 */
	const image_read_format*
	get_most_suitable_format(const image_probe &probe) const;

private:
	class implementation;
	VITRIO_STD_MEMBER_INTERFACE
	std::unique_ptr<implementation> m_implementation;
};

} // namespace vitrio
