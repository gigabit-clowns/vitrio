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
	// Only the process that opened the ring has transfers in it to wait for.
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
			// The kernel is short of memory for the transfers that are
			// queued, and may have it later. Failing otherwise, it could
			// still write into the buffers of reads, which nothing could then
			// release safely.
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
	auto &read = take_request();
	read.kind = direction::read;
	read.file = file;
	read.offset = offset;
	read.tag = tag;
	read.buffers.assign(buffers.begin(), buffers.end());
	dispatch(read);
}

void io_uring_file_transfer_queue::queue_write(
	file_handle_t file,
	std::uint64_t offset,
	span<const const_memory_range> buffers,
	std::uint64_t tag
)
{
	auto &write = take_request();
	write.kind = direction::write;
	write.file = file;
	write.offset = offset;
	write.tag = tag;
	write.buffers.clear();
	for (const auto &buffer : buffers)
	{
		write.buffers.emplace_back(
			const_cast<void*>(buffer.get_address()),
			buffer.get_size()
		);
	}
	dispatch(write);
}

void io_uring_file_transfer_queue::queue_sync(
	file_handle_t file,
	std::uint64_t tag
)
{
	auto &sync = take_request();
	sync.kind = direction::sync;
	sync.file = file;
	sync.offset = 0;
	sync.tag = tag;
	sync.buffers.clear();
	dispatch(sync);
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

io_uring_file_transfer_queue::request&
io_uring_file_transfer_queue::take_request()
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

	return *m_free_requests.back();
}

void io_uring_file_transfer_queue::dispatch(request &transfer)
{
	// The kernel takes as many buffers in one transfer as a readv does, so a
	// transfer over more is handed to it a part at a time.
	transfer.done = 0;
	transfer.vectors.resize(
		std::min<std::size_t>(transfer.buffers.size(), IOV_MAX)
	);

	if (send(transfer))
	{
		m_free_requests.pop_back();
	}
	else
	{
		m_completions.emplace_back(transfer.tag, std::size_t{0});
	}
}

bool io_uring_file_transfer_queue::send(request &transfer) noexcept
{
	std::size_t count = 0;
	if (transfer.kind != direction::sync)
	{
		count = gather_iovecs(
			make_span(transfer.buffers),
			transfer.done,
			make_span(transfer.vectors),
			std::numeric_limits<std::size_t>::max()
		);
		if (count == 0)
		{
			return false;
		}
	}

	// The ring has an entry for every request.
	auto *entry = io_uring_get_sqe(&m_ring);
	VITRIO_ASSERT(entry != nullptr);

	switch (transfer.kind)
	{
	case direction::read:
		io_uring_prep_readv(
			entry,
			transfer.file,
			transfer.vectors.data(),
			static_cast<unsigned>(count),
			transfer.offset + transfer.done
		);
		break;
	case direction::write:
		io_uring_prep_writev(
			entry,
			transfer.file,
			transfer.vectors.data(),
			static_cast<unsigned>(count),
			transfer.offset + transfer.done
		);
		break;
	case direction::sync:
		io_uring_prep_fsync(entry, transfer.file, 0);
		break;
	}
	io_uring_sqe_set_data(entry, &transfer);
	return true;
}

file_transfer_completion io_uring_file_transfer_queue::receive()
{
	for (;;)
	{
		// One call submits what is queued and waits. A signal may cut the
		// wait short once the transfers are submitted, so it is then waited
		// on again.
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

		auto &transfer =
			*static_cast<request*>(io_uring_cqe_get_data(entry));
		const auto result = entry->res;
		io_uring_cqe_seen(&m_ring, entry);

		if (result < 0)
		{
			m_free_requests.push_back(&transfer);
			return file_transfer_completion(
				transfer.tag,
				std::error_code(-result, std::system_category())
			);
		}

		if (transfer.kind == direction::sync)
		{
			m_free_requests.push_back(&transfer);
			return file_transfer_completion(transfer.tag, std::size_t{0});
		}

		// A write that moves nothing would move nothing again: the system
		// gave no reason, and none is made up.
		if (result == 0 && transfer.kind == direction::write)
		{
			m_free_requests.push_back(&transfer);
			return file_transfer_completion(
				transfer.tag,
				std::make_error_code(std::errc::io_error)
			);
		}

		// The kernel may move less than it was asked to, or have been asked
		// for a part, and is then asked for what is left. A read moves
		// nothing only at the end of the file.
		transfer.done += static_cast<std::size_t>(result);
		if (result == 0 || !send(transfer))
		{
			m_free_requests.push_back(&transfer);
			return file_transfer_completion(transfer.tag, transfer.done);
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
