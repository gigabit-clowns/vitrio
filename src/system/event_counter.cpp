// SPDX-License-Identifier: LGPL-2.1-or-later

#include "event_counter.hpp"

#include <assert.hpp>

#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <system_error>

namespace vitrio
{

event_counter::event_counter()
	: m_file(::eventfd(0, EFD_CLOEXEC))
{
	if (m_file < 0)
	{
		throw std::system_error(errno, std::system_category(), "eventfd");
	}
}

event_counter::~event_counter()
{
	::close(m_file);
}

file_handle_t event_counter::get_file() const noexcept
{
	return m_file;
}

void event_counter::signal() noexcept
{
	// A write fails only if the counter would overflow, which reads that
	// reset it keep it far from, or if a signal interrupts it.
	const std::uint64_t one = 1;
	ssize_t written;
	do
	{
		written = ::write(m_file, &one, sizeof(one));
	}
	while (written < 0 && errno == EINTR);

	VITRIO_ASSERT(written == static_cast<ssize_t>(sizeof(one)));
}

} // namespace vitrio
