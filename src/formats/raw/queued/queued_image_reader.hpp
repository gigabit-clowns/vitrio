// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_executor.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

/**
 * @brief Reads a file whose values are laid out as a @ref raw_image_layout
 * states, through reads queued by an executor of file transfers.
 *
 * The file is opened when the reader is constructed, and everything the
 * reader reports is settled then. A read is planned on the thread that starts
 * it, as @ref make_image_read_operation plans it, and carried out by the
 * executor, so that many reads, of this reader and of others, are in flight
 * together. A read keeps the file open until it reports, even after the
 * reader is gone.
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
	 * @param transfers What carries out the reads. Never null.
	 * @throws image_file_error If the file can not be opened.
	 * @throws image_file_format_error If the file is shorter than @p layout
	 * says it is.
	 */
	queued_image_reader(
		const std::string &path,
		raw_image_layout layout,
		image_metadata metadata,
		std::shared_ptr<file_transfer_executor> transfers
	);

	~queued_image_reader() override;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const override;

private:
	std::string m_path;

	// Boost opens a file for mapping on every system without mapping it,
	// which is all a reader that queues positioned reads of it needs. The
	// reads under way share it.
	std::shared_ptr<const boost::interprocess::file_mapping> m_file;
	std::size_t m_file_size;

	raw_image_layout m_layout;
	image_metadata m_metadata;
	std::shared_ptr<file_transfer_executor> m_transfers;
};

} // namespace vitrio
