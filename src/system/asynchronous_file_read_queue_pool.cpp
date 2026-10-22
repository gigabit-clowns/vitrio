// SPDX-License-Identifier: LGPL-2.1-or-later

#include "asynchronous_file_read_queue_pool.hpp"

#include "blocking_file_read_queue.hpp"

#if defined(VITRIO_ENABLE_IO_URING)
	#include "io_uring_file_read_queue.hpp"
#endif

#include <assert.hpp>
#include <config.hpp>
#include <logger.hpp>

#include <exception>
#include <memory>
#include <system_error>

namespace vitrio
{

namespace
{

// A queue whose reads the kernel carries out while the thread goes on, which
// only builds with io_uring have.
std::unique_ptr<file_read_queue> open_asynchronous_queue()
{
#if defined(VITRIO_ENABLE_IO_URING)
	return std::make_unique<io_uring_file_read_queue>(
		VITRIO_FILE_READ_QUEUE_CAPACITY
	);
#else
	throw std::system_error(
		std::make_error_code(std::errc::function_not_supported),
		"This build of vitrio reads no file asynchronously"
	);
#endif
}

std::unique_ptr<file_read_queue> open_queue()
{
	try
	{
		return open_asynchronous_queue();
	}
	catch (const std::system_error &error)
	{
		// The system may refuse one more queue, short of the memory it lets
		// the process lock or of the files it lets it open. The thread then
		// reads as it is asked.
		VITRIO_LOG_WARN(
			"A thread could not open a queue of asynchronous file reads ({}), "
			"and reads its files synchronously instead.",
			error.what()
		);
		return std::make_unique<blocking_file_read_queue>();
	}
}

} // anonymous namespace

asynchronous_file_read_queue_pool::asynchronous_file_read_queue_pool()
	noexcept
{
	VITRIO_ASSERT(is_supported());
}

asynchronous_file_read_queue_pool::~asynchronous_file_read_queue_pool() =
	default;

bool asynchronous_file_read_queue_pool::is_supported() noexcept
{
	// Opening a queue is what tells whether the process can have them.
	static const bool supported = []() noexcept
	{
		try
		{
			open_asynchronous_queue();
			return true;
		}
		catch (const std::exception &)
		{
			return false;
		}
	}();

	return supported;
}

file_read_queue& asynchronous_file_read_queue_pool::get_queue()
{
	thread_local const auto queue = open_queue();
	return *queue;
}

} // namespace vitrio
