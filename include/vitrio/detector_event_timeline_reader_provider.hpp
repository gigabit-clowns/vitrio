// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class detector_event_timeline_reader;

/**
 * @brief Interface to serve @ref detector_event_timeline_reader given a key.
 *
 * What a key is and how the reader is created are left to the
 * implementation. A key is often the path of a file, and need not be.
 *
 * @par Thread safety
 * A provider may be asked for readers concurrently.
 *
 * @see image_reader_provider
 */
class VITRIO_API detector_event_timeline_reader_provider
{
public:
	detector_event_timeline_reader_provider() noexcept;
	detector_event_timeline_reader_provider(
		const detector_event_timeline_reader_provider &other
	) = delete;
	detector_event_timeline_reader_provider(
		detector_event_timeline_reader_provider &&other
	) = delete;
	virtual ~detector_event_timeline_reader_provider();

	detector_event_timeline_reader_provider& operator=(
		const detector_event_timeline_reader_provider &other
	) = delete;
	detector_event_timeline_reader_provider& operator=(
		detector_event_timeline_reader_provider &&other
	) = delete;

	/**
	 * @brief Get the reader a key names.
	 *
	 * @param key Key of the acquisition to read, such as the path of its
	 * file.
	 * @return std::shared_ptr<const detector_event_timeline_reader> The
	 * reader, never null.
	 * @throws image_file_error If the file does not exist or can not be read.
	 * @throws unsupported_operation_error If no format can read the file.
	 * @throws image_file_format_error If the file is malformed or truncated.
	 */
	virtual std::shared_ptr<const detector_event_timeline_reader>
	acquire(const std::string &key) = 0;
};

} // namespace vitrio
