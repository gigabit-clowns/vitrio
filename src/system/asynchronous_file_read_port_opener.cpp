// SPDX-License-Identifier: LGPL-2.1-or-later

#include "asynchronous_file_read_port_opener.hpp"

#include "blocking_file_read_port.hpp"

#if defined(VITRIO_ENABLE_IO_URING)
	#include "io_uring_file_read_port.hpp"
#endif

#include <assert.hpp>
#include <logger.hpp>

#include <exception>
#include <system_error>

namespace vitrio
{

namespace
{

// A port whose reads the kernel carries out while the thread goes on, which
// only builds with io_uring have.
std::unique_ptr<file_read_port> open_asynchronous_port(std::size_t capacity)
{
#if defined(VITRIO_ENABLE_IO_URING)
	return std::make_unique<io_uring_file_read_port>(capacity);
#else
	static_cast<void>(capacity);
	throw std::system_error(
		std::make_error_code(std::errc::function_not_supported),
		"This build of vitrio reads no file asynchronously"
	);
#endif
}

} // anonymous namespace

asynchronous_file_read_port_opener::asynchronous_file_read_port_opener(
	std::size_t capacity
) noexcept
	: m_capacity(capacity)
{
	VITRIO_ASSERT(capacity > 0);
	VITRIO_ASSERT(is_supported());
}

asynchronous_file_read_port_opener::~asynchronous_file_read_port_opener() =
	default;

bool asynchronous_file_read_port_opener::is_supported() noexcept
{
	// Opening a port is what tells whether the process can have them.
	static const bool supported = []() noexcept
	{
		try
		{
			open_asynchronous_port(1);
			return true;
		}
		catch (const std::exception &)
		{
			return false;
		}
	}();

	return supported;
}

std::unique_ptr<file_read_port>
asynchronous_file_read_port_opener::open() const
{
	try
	{
		return open_asynchronous_port(m_capacity);
	}
	catch (const std::system_error &error)
	{
		// The system may refuse one more, short of the memory it lets the
		// process lock or of the files it lets it open.
		VITRIO_LOG_WARN(
			"A port of asynchronous file reads could not be opened ({}), and "
			"its reads are carried out as they are asked instead.",
			error.what()
		);
		return std::make_unique<blocking_file_read_port>();
	}
}

} // namespace vitrio
