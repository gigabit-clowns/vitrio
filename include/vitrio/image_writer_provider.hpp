// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_writer;

/**
 * @brief Interface to serve @ref image_writer given a key.
 *
 * What a key is and how the @ref image_writer is created are left to the
 * implementation. A key is often the path of a file, and need not be.
 *
 * @par Thread safety
 * A provider may be asked for writers concurrently.
 */
class VITRIO_API image_writer_provider
{
public:
	image_writer_provider() noexcept;
	image_writer_provider(const image_writer_provider &other) = delete;
	image_writer_provider(image_writer_provider &&other) = delete;
	virtual ~image_writer_provider();

	image_writer_provider&
	operator=(const image_writer_provider &other) = delete;
	image_writer_provider&
	operator=(image_writer_provider &&other) = delete;

	/**
	 * @brief Get the writer a key names.
	 *
	 * Shared ownership rather than a reference, so that a writer the
	 * provider stops keeping stays alive for as long as it is still written
	 * through.
	 *
	 * @param key Key of the file to write, such as its path.
	 * @return std::shared_ptr<image_writer> The writer, never null.
	 * @throws std::out_of_range If this provider serves no such file.
	 * @throws unsupported_operation_error If no format can create the file,
	 * or the chosen one can not represent it.
	 * @throws image_file_error If the file could not be created.
	 */
	virtual std::shared_ptr<image_writer>
	acquire(const std::string &key) = 0;
};

} // namespace vitrio
