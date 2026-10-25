// SPDX-License-Identifier: LGPL-2.1-or-later

#include "io_uring_file_transfer_queue.hpp"

#include "iovec_gathering.hpp"
#include "unseen_mapping_annotation.hpp"

#include <assert.hpp>

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <exception>
#include <limits>
#include <system_error>

namespace vitrio
{

namespace
{

[[noreturn]] void throw_system_error(int status, const char *call)
{
	throw std::system_error(-status, std::system_category(), call);
}

} // anonymous namespace

io_uring_file_transfer_queue::io_uring_file_transfer_queue(std::size_t capacity)
	: m_requests(capacity)
	, m_process(0)
{
	VITRIO_ASSERT(capacity > 0);

	m_free_requests.reserve(capacity);
	for (auto &request : m_requests)
	{
		m_free_requests.push_back(&request);
	}

	open_ring();
}

io_uring_file_transfer_queue::~io_uring_file_transfer_queue()
{
	// Only the process that opened the ring has reads in it to wait for.
	while (
		m_process == ::getpid() &&
		m_free_requests.size() < m_requests.size()
	)
	{
		try
		{
			receive();
		}
		catch (const std::system_error &error)
		{
			// The kernel is short of memory for the reads that are queued,
			// and may have it later. Failing otherwise, it could still write
			// into their buffers, which nothing could then release safely.
			const auto code = error.code();
			if (
				code != std::errc::resource_unavailable_try_again &&
				code != std::errc::device_or_resource_busy
			)
			{
				std::terminate();
			}
		}
	}

	close_ring();
}

void io_uring_file_transfer_queue::queue_read(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::uint64_t tag
)
{
	// The child of a fork shares the memory of an inherited ring with its
	// parent. Closing its copy leaves the parent's open.
	if (m_process != ::getpid())
	{
		VITRIO_ASSERT(m_free_requests.size() == m_requests.size());
		VITRIO_ASSERT(m_completions.empty());
		close_ring();
		open_ring();
	}

	if (m_free_requests.empty())
	{
		m_completions.push_back(receive());
	}

	// The kernel takes as many buffers in one read as a readv does, so a read
	// over more is handed to it a part at a time.
	auto &read = *m_free_requests.back();
	read.file = file;
	read.offset = offset;
	read.tag = tag;
	read.buffers.assign(buffers.begin(), buffers.end());
	read.done = 0;
	read.vectors.resize(std::min<std::size_t>(buffers.size(), IOV_MAX));

	if (send(read))
	{
		m_free_requests.pop_back();
	}
	else
	{
		m_completions.emplace_back(tag, std::size_t{0});
	}
}

file_transfer_completion io_uring_file_transfer_queue::wait()
{
	if (m_completions.empty())
	{
		VITRIO_ASSERT(m_free_requests.size() < m_requests.size());
		return receive();
	}

	submit();
	const auto completion = m_completions.front();
	m_completions.pop_front();
	return completion;
}

bool io_uring_file_transfer_queue::send(request &read) noexcept
{
	const auto count = gather_iovecs(
		make_span(read.buffers),
		read.done,
		make_span(read.vectors),
		std::numeric_limits<std::size_t>::max()
	);
	if (count == 0)
	{
		return false;
	}

	// The ring has an entry for every request.
	auto *entry = io_uring_get_sqe(&m_ring);
	VITRIO_ASSERT(entry != nullptr);

	io_uring_prep_readv(
		entry,
		read.file,
		read.vectors.data(),
		static_cast<unsigned>(count),
		read.offset + read.done
	);
	io_uring_sqe_set_data(entry, &read);
	return true;
}

file_transfer_completion io_uring_file_transfer_queue::receive()
{
	for (;;)
	{
		// One call submits what is queued and waits. A signal may cut the
		// wait short once the reads are submitted, so it is then waited on
		// again.
		int status = 0;
		if (io_uring_sq_ready(&m_ring) > 0)
		{
			do
			{
				status = io_uring_submit_and_wait(&m_ring, 1);
			}
			while (status == -EINTR);

			if (status < 0)
			{
				throw_system_error(status, "io_uring_submit_and_wait");
			}
		}

		io_uring_cqe *entry = nullptr;
		do
		{
			status = io_uring_wait_cqe(&m_ring, &entry);
		}
		while (status == -EINTR);

		if (status < 0)
		{
			throw_system_error(status, "io_uring_wait_cqe");
		}

		auto &read = *static_cast<request*>(io_uring_cqe_get_data(entry));
		const auto result = entry->res;
		io_uring_cqe_seen(&m_ring, entry);

		if (result < 0)
		{
			m_free_requests.push_back(&read);
			return file_transfer_completion(
				read.tag,
				std::error_code(-result, std::system_category())
			);
		}

		// The kernel may read less than it was asked for, or have been asked
		// for a part, and is then asked for what is left. It reads nothing
		// only at the end of the file.
		read.done += static_cast<std::size_t>(result);
		if (result == 0 || !send(read))
		{
			m_free_requests.push_back(&read);
			return file_transfer_completion(read.tag, read.done);
		}
	}
}

void io_uring_file_transfer_queue::submit()
{
	if (io_uring_sq_ready(&m_ring) == 0)
	{
		return;
	}

	int status;
	do
	{
		status = io_uring_submit(&m_ring);
	}
	while (status == -EINTR);

	if (status < 0)
	{
		throw_system_error(status, "io_uring_submit");
	}
}

void io_uring_file_transfer_queue::open_ring()
{
	// The ring has an entry for each request, and twice as many for
	// completions, so that neither ever fills.
	const auto status = io_uring_queue_init(
		static_cast<unsigned>(m_requests.size()),
		&m_ring,
		0
	);
	if (status < 0)
	{
		throw_system_error(status, "io_uring_queue_init");
	}

	announce_unseen_mapping();
	m_process = ::getpid();
}

void io_uring_file_transfer_queue::close_ring() noexcept
{
	if (m_process != 0)
	{
		announce_unseen_unmapping();
		io_uring_queue_exit(&m_ring);
		m_process = 0;
	}
}

} // namespace vitrio
