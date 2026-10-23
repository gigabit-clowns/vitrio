// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class image_file_read_format_selector;
class file_transfer_pool;

/**
 * @brief A provider that reads files. It opens each file through a format
 * selector, every time it is asked for one.
 *
 * It keeps nothing: every reader it returns is newly opened through the
 * format selector it was constructed with, so a key asked for twice is
 * opened twice and the two readers are unrelated. With no capacity to size
 * and no eviction, a file is opened exactly as often as it is asked for.
 *
 * A key is handed to the selector as the path of the file, and the readers
 * carry out their transfers on the pool the provider was given.
 *
 * @see file_image_writer_provider
 * @see image_file_read_format_selector
 */
class VITRIO_API file_image_reader_provider final
	: public image_reader_provider
{
public:
	/**
	 * @brief Construct a provider opening through a format selector.
	 *
	 * @param formats The formats a file may be opened with.
	 * @param transfers The threads that carry out the transfers of the
	 * readers.
	 * @throws std::invalid_argument If @p formats or @p transfers is null.
	 */
	file_image_reader_provider(
		std::shared_ptr<const image_file_read_format_selector> formats,
		std::shared_ptr<const file_transfer_pool> transfers
	);

	~file_image_reader_provider() override;

	std::shared_ptr<const image_reader>
	acquire(const std::string &key) override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<const image_file_read_format_selector> m_formats;

	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<const file_transfer_pool> m_transfers;
};

} // namespace vitrio
