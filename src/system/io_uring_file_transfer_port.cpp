// SPDX-License-Identifier: LGPL-2.1-or-later

#include "io_uring_file_transfer_port.hpp"

#include <assert.hpp>

#include <system_error>

namespace vitrio
{

namespace
{

// The tag the ring knows the read of the counter by.
constexpr std::uint64_t wakeup_tag = 0;

} // anonymous namespace

io_uring_file_transfer_port::io_uring_file_transfer_port(std::size_t capacity)
	: m_wakeup_count(0)
	, m_tags(1, 0)
	, m_queue(capacity + 1)
{
	VITRIO_ASSERT(capacity > 0);

	queue_wakeup();
}

io_uring_file_transfer_port::~io_uring_file_transfer_port()
{
	// The queue waits for its transfers as it is destroyed, the read of the
	// counter among them.
	m_wakeups.signal();
}

template <typename Queue>
void io_uring_file_transfer_port::queue_tagged(
	std::uint64_t tag,
	Queue &&queue
)
{
	if (m_free_tags.empty())
	{
		// The free list gets room for every tag up front, so that freeing
		// one as its transfer completes never fails.
		m_free_tags.reserve(m_tags.size() + 1);
		m_tags.push_back(tag);
		try
		{
			queue(m_tags.size() - 1);
		}
		catch (...)
		{
			m_tags.pop_back();
			throw;
		}
		return;
	}

	// The tag is taken only once the transfer is, so a transfer the system
	// refuses leaves it free.
	const auto ring_tag = m_free_tags.back();
	queue(ring_tag);
	m_free_tags.pop_back();
	m_tags[ring_tag] = tag;
}

void io_uring_file_transfer_port::queue_read(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::uint64_t tag
)
{
	queue_tagged(
		tag,
		[&](std::uint64_t ring_tag)
		{
			m_queue.queue_read(file, offset, buffers, ring_tag);
		}
	);
}

void io_uring_file_transfer_port::queue_write(
	file_handle_t file,
	std::uint64_t offset,
	span<const const_memory_range> buffers,
	std::uint64_t tag
)
{
	queue_tagged(
		tag,
		[&](std::uint64_t ring_tag)
		{
			m_queue.queue_write(file, offset, buffers, ring_tag);
		}
	);
}

void io_uring_file_transfer_port::queue_sync(
	file_handle_t file,
	std::uint64_t tag
)
{
	queue_tagged(
		tag,
		[&](std::uint64_t ring_tag)
		{
			m_queue.queue_sync(file, ring_tag);
		}
	);
}

void io_uring_file_transfer_port::wake() noexcept
{
	m_wakeups.signal();
}

boost::optional<file_transfer_completion> io_uring_file_transfer_port::wait()
{
	const auto completion = m_queue.wait();
	const auto ring_tag = completion.get_tag();

	if (ring_tag == wakeup_tag)
	{
		queue_wakeup();
		if (completion.get_error())
		{
			throw std::system_error(
				completion.get_error(),
				"read of an eventfd"
			);
		}
		return boost::none;
	}

	const auto tag = m_tags[ring_tag];
	m_free_tags.push_back(ring_tag);

	if (completion.get_error())
	{
		return file_transfer_completion(tag, completion.get_error());
	}
	return file_transfer_completion(tag, completion.get_size());
}

void io_uring_file_transfer_port::queue_wakeup()
{
	const memory_range buffer(&m_wakeup_count, sizeof(m_wakeup_count));
	m_queue.queue_read(
		m_wakeups.get_file(),
		0,
		make_span(&buffer, 1),
		wakeup_tag
	);
}

} // namespace vitrio
