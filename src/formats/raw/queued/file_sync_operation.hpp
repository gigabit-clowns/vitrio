// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "writable_image_file.hpp"

#include <file_transfer/file_transfer_operation.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief Makes what was written to a file reach the storage: a sync of it,
 * which covers the writes that completed before the operation started.
 *
 * The operation keeps the file open until it is done.
 */
class file_sync_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Prepare the sync of a file.
	 *
	 * @param file The file, open for writing. Never null.
	 */
	explicit file_sync_operation(std::shared_ptr<writable_image_file> file);

	~file_sync_operation() override;

	void start(file_transfer_scope &scope) override;

	/**
	 * @throws image_file_error If the system failed the sync.
	 */
	void on_transfer(
		const file_transfer_completion &completion,
		file_transfer_scope &scope
	) override;

	void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) override;

private:
	std::shared_ptr<writable_image_file> m_file;
};

} // namespace vitrio
