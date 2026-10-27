// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "writable_image_file.hpp"

#include <file_transfer/file_transfer_executor.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_writer.hpp>
#include <vitrio/span.hpp>

#include <memory>
#include <string>

namespace vitrio
{

/**
 * @brief Creates a file whose values are laid out as a @ref raw_image_layout
 * states, and writes it through writes queued by an executor of file
 * transfers.
 *
 * The layout is complete when a writer is constructed, so the file is laid
 * out in full there and then: sized, and what precedes its values written.
 * A region can be written wherever it belongs and in any order afterwards.
 * A write is planned on the thread that starts it, as
 * @ref make_image_write_operation plans it, and carried out by the executor,
 * as a flush is, so that many writes, of this writer and of others, are in
 * flight at once.
 *
 * A region that is never written keeps whatever the file was laid out with,
 * which is unspecified.
 *
 * A writer that is destroyed flushes itself as @ref flush_async does,
 * without waiting: the sync is carried out by the executor, which keeps the
 * file open until then, and a failure of it can only be logged. A writer
 * whose failures matter is flushed beforehand.
 */
class queued_image_writer final
	: public image_writer
{
public:
	/**
	 * @brief Create a file and open it for writing.
	 *
	 * Any file already at that path is replaced.
	 *
	 * @param path Path to the file to create.
	 * @param layout Where and how the file holds its values.
	 * @param preamble The bytes the file begins with, before its values.
	 * @param transfers What carries out the writes. Never null.
	 * @throws std::invalid_argument If @p preamble reaches past where
	 * @p layout says the values begin.
	 * @throws image_file_error If the file could not be created, sized,
	 * opened or begun.
	 */
	queued_image_writer(
		const std::string &path,
		raw_image_layout layout,
		span<const byte> preamble,
		std::shared_ptr<file_transfer_executor> transfers
	);

	/**
	 * @brief Hand the executor a sync of the file, which covers the writes
	 * that reported before, as a flush does.
	 */
	~queued_image_writer() override;

	const image_descriptor& get_descriptor() const noexcept override;

	void write_async(
		const_array source,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) override;

	void flush_async(std::shared_ptr<completion_notifier> done) override;

private:
	raw_image_layout m_layout;

	// The writes under way share it, and the last to let it go closes it.
	std::shared_ptr<writable_image_file> m_file;

	std::shared_ptr<file_transfer_executor> m_transfers;
};

} // namespace vitrio
