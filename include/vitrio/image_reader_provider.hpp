// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_descriptor.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_reader;

/**
 * @brief Interface to serve @ref image_reader given a key.
 *
 * What a key is and how the @ref image_reader is created are left to the
 * implementation. A key is often the path of a file, and need not be.
 *
 * @par Thread safety
 * A provider may be asked for readers concurrently.
 */
class VITRIO_API image_reader_provider
{
public:
	image_reader_provider() noexcept;
	image_reader_provider(const image_reader_provider &other) = delete;
	image_reader_provider(image_reader_provider &&other) = delete;
	virtual ~image_reader_provider();

	image_reader_provider&
	operator=(const image_reader_provider &other) = delete;
	image_reader_provider&
	operator=(image_reader_provider &&other) = delete;

	/**
	 * @brief Get the reader a key names.
	 *
	 * Shared ownership rather than a reference, so that a reader the
	 * provider stops keeping stays alive for as long as it is still read
	 * through.
	 *
	 * @param key Key of the file to read, such as its path.
	 * @return std::shared_ptr<const image_reader> The reader, never null.
	 * @throws image_file_error If the file does not exist or can not be read.
	 * @throws unsupported_operation_error If no format can read the file.
	 * @throws image_format_error If the file is malformed or truncated.
	 */
	virtual std::shared_ptr<const image_reader>
	acquire(const std::string &key) = 0;
};

/**
 * @brief Get the descriptor of a file through a provider.
 *
 * A copy the caller owns, so the shape of a file is answered without the
 * reader being kept. Whether the file is opened to answer depends on
 * @p readers, which may already hold a reader over it.
 *
 * @param readers Where the file becomes a reader.
 * @param key Key of the file in @p readers, such as its path.
 * @return image_descriptor The descriptor of the file.
 * @throws image_file_error If the file does not exist or can not be read.
 * @throws unsupported_operation_error If no format can read the file.
 * @throws image_format_error If the file is malformed or truncated.
 */
VITRIO_API
image_descriptor query_descriptor(
	image_reader_provider &readers,
	const std::string &key
);

} // namespace vitrio
