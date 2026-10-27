// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_reader.hpp>

namespace vitrio
{

class array_ref;

/**
 * @brief A reader that reads on the calling thread.
 *
 * Its @ref read is done before it returns, and @ref read_async is that read,
 * reported to its notifier before it returns. A reader whose work belongs
 * on the thread that asks for it, such as one decoding a file through a
 * library that reads it, implements @ref read alone.
 */
class VITRIO_API synchronous_image_reader
	: public image_reader
{
public:
	synchronous_image_reader() noexcept;
	~synchronous_image_reader() override;

	/**
	 * @copydoc image_reader::read_async
	 *
	 * Reads as @ref read does, and reports to @p done before returning.
	 */
	void read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const final;

	/**
	 * @brief Read a set of hyperrectangles of the file into one array, all
	 * before returning.
	 *
	 * As @ref image_reader::read_async describes, except that it is done
	 * when it returns and a failure is thrown. @p destination must outlive
	 * the call, and no longer.
	 *
	 * @param destination Where the regions are written. Must be
	 * initialized.
	 * @param regions The regions to read and where each one lands.
	 * @throws std::invalid_argument, std::out_of_range,
	 * unsupported_operation_error, image_file_format_error, image_file_error
	 * As @ref image_reader::read_async reports them.
	 */
	virtual void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const = 0;
};

} // namespace vitrio
