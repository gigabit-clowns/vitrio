// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_sync_operation.hpp"

#include <vitrio/exceptions/image_file_error.hpp>

#include <assert.hpp>

#include <utility>

namespace vitrio
{

file_sync_operation::file_sync_operation(
	std::shared_ptr<writable_image_file> file
)
	: m_file(std::move(file))
{
	VITRIO_ASSERT(m_file);
}

file_sync_operation::~file_sync_operation() = default;

void file_sync_operation::start(file_transfer_scope &scope)
{
	scope.queue_sync(m_file->get_file(), 0);
}

void file_sync_operation::on_transfer(
	const file_transfer_completion &completion,
	file_transfer_scope &
)
{
	if (completion.get_error())
	{
		throw image_file_error(
			m_file->get_path() + ": file_sync_operation: What was written "
			"could not reach the storage: " + completion.get_error().message()
		);
	}
}

void file_sync_operation::on_task(
	std::uint64_t,
	std::exception_ptr,
	file_transfer_scope &
)
{
	// It runs no task.
	VITRIO_ASSERT(false);
}

} // namespace vitrio
