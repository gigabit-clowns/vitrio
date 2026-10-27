// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_writer.hpp>

namespace vitrio
{

class const_array_ref;

/**
 * @brief A writer that writes on the calling thread.
 *
 * Its @ref write and @ref flush are done before they return, and
 * @ref write_async and @ref flush_async are those, reported to their
 * notifiers before they return. A writer whose work belongs on the thread
 * that asks for it implements @ref write and @ref flush alone.
 */
class VITRIO_API synchronous_image_writer
	: public image_writer
{
public:
	synchronous_image_writer() noexcept;
	~synchronous_image_writer() override;

	/**
	 * @copydoc image_writer::write_async
	 *
	 * Writes as @ref write does, and reports to @p done before returning.
	 */
	void write_async(
		const_array source,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) final;

	/**
	 * @copydoc image_writer::flush_async
	 *
	 * Flushes as @ref flush does, and reports to @p done before returning.
	 */
	void flush_async(std::shared_ptr<completion_notifier> done) final;

	/**
	 * @brief Write a set of hyperrectangles of the file from one array, all
	 * before returning.
	 *
	 * As @ref image_writer::write_async describes, except that it is done
	 * when it returns and a failure is thrown. @p source must outlive the
	 * call, and no longer.
	 *
	 * @param source The values to write. Must be initialized.
	 * @param regions The regions to write and where each one comes from.
	 * @throws std::invalid_argument, std::out_of_range,
	 * unsupported_operation_error, image_file_error As
	 * @ref image_writer::write_async reports them.
	 */
	virtual void write(
		const_array_ref source,
		const image_transfer_plan &regions
	) = 0;

	/**
	 * @brief Make what has been written reach the storage, before
	 * returning.
	 *
	 * @throws image_file_error If what has been written could not reach the
	 * storage.
	 */
	virtual void flush() = 0;
};

} // namespace vitrio
