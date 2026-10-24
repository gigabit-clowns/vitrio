// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_pool_implementation.hpp"

#include <file_transfer/thread_pool_file_transfer_executor.hpp>
#include <system/asynchronous_file_read_port_opener.hpp>
#include <system/blocking_file_read_port_opener.hpp>
#include <system/file_read_port_opener.hpp>

#include <assert.hpp>
#include <tuning.hpp>

#include <utility>

namespace vitrio
{

namespace
{

// What opens the ports of the threads: one of asynchronous ports where the
// process can have them, and of ports that read as they are asked otherwise.
std::unique_ptr<file_read_port_opener> make_port_opener()
{
	if (asynchronous_file_read_port_opener::is_supported())
	{
		return std::make_unique<asynchronous_file_read_port_opener>(
			get_file_read_queue_capacity()
		);
	}

	return std::make_unique<blocking_file_read_port_opener>();
}

} // anonymous namespace

file_transfer_pool::implementation::implementation(
	std::shared_ptr<executor> tasks
)
	: m_executor(
		std::make_shared<thread_pool_file_transfer_executor>(
			std::move(tasks),
			*make_port_opener(),
			get_file_transfer_workers()
		)
	)
{
}

file_transfer_pool::implementation::~implementation() = default;

const std::shared_ptr<file_transfer_executor>&
file_transfer_pool::implementation::get_executor() const noexcept
{
	return m_executor;
}

} // namespace vitrio
