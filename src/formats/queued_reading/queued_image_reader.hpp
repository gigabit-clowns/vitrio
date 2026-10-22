// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/raw_image_layout.hpp>

#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>

#include <system/file_read_queue_pool.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

/**
 * @brief Reads a file whose values are laid out as a @ref raw_image_layout
 * states, through the queue of file reads of the thread that reads.
 *
 * The file is opened when the reader is constructed, and everything the
 * reader reports is settled then. A read takes the queue of its thread from a
 * pool, so reads may run concurrently, each with a queue of its own, while
 * they share the file. What a read does is @ref read_queued_regions.
 */
class queued_image_reader final
	: public image_reader
{
public:
	/**
	 * @brief Open a file for reading.
	 *
	 * @param path Path to the file.
	 * @param layout Where and how the file holds its values.
	 * @param metadata What the file states beyond its shape and data type.
	 * @param queues The pool the threads that read take their queues from.
	 * Never null.
	 * @throws image_file_error If the file can not be opened.
	 * @throws image_file_format_error If the file is shorter than @p layout
	 * says it is.
	 */
	queued_image_reader(
		const std::string &path,
		raw_image_layout layout,
		image_metadata metadata,
		std::shared_ptr<file_read_queue_pool> queues
	);

	~queued_image_reader() override = default;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

private:
	std::string m_path;

	// Boost opens a file for mapping on every system without mapping it,
	// which is all a reader that queues positioned reads of it needs.
	boost::interprocess::file_mapping m_file;
	std::size_t m_file_size;

	raw_image_layout m_layout;
	image_metadata m_metadata;
	std::shared_ptr<file_read_queue_pool> m_queues;
};

} // namespace vitrio
