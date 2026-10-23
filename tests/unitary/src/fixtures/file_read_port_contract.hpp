// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>

#include "mapped_test_file.hpp"

#include <system/file_read_completion.hpp>
#include <system/file_read_port.hpp>
#include <system/file_read_queue.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace vitrio
{

/**
 * @brief A port waited on as a queue is: one that nothing wakes returns
 * completions only, so it keeps the contract of a @ref file_read_queue.
 */
class port_file_read_queue final
	: public file_read_queue
{
public:
	explicit port_file_read_queue(std::unique_ptr<file_read_port> port)
		: m_port(std::move(port))
	{
	}

	void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override
	{
		m_port->queue(file, offset, buffers, tag);
	}

	file_read_completion wait() override
	{
		const auto completion = m_port->wait();
		if (!completion)
		{
			throw std::logic_error("A port nothing woke returned no read.");
		}
		return *completion;
	}

private:
	std::unique_ptr<file_read_port> m_port;
};

/**
 * @brief Check that the ports a function makes are woken as the contract of
 * a @ref file_read_port says, in a section each.
 *
 * What they share with a queue is checked by running the contract of a
 * queue on them through a @ref port_file_read_queue.
 *
 * @param name The name of the file the ports read, which no other case of
 * the suite may use.
 * @param make_port Makes a port with no read outstanding, as a
 * std::unique_ptr.
 */
template <typename MakePort>
void check_file_read_port_contract(const std::string &name, MakePort make_port)
{
	constexpr std::size_t file_size = 4000;
	const mapped_test_file file(name, file_size);

	// The buffers outlive the port, which may still be reading into them
	// until it is destroyed.
	std::vector<std::vector<byte>> buffers(3, std::vector<byte>(100));
	auto port = make_port();

	SECTION( "a wake from another thread ends a wait with no read" )
	{
		std::thread waker(
			[&]
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				port->wake();
			}
		);

		const auto woken = port->wait();
		waker.join();
		CHECK_FALSE( woken.has_value() );
	}

	SECTION( "a wake before a wait keeps it from blocking" )
	{
		port->wake();
		CHECK_FALSE( port->wait().has_value() );
	}

	SECTION( "a wake among reads is answered, and so is every read" )
	{
		for (std::uint64_t tag = 0; tag < buffers.size(); ++tag)
		{
			const memory_range buffer(buffers[tag].data(), 100);
			port->queue(
				file.get_file(),
				1000 * tag,
				make_span(&buffer, 1),
				tag
			);
		}
		port->wake();

		// The waits do not block until the wake is answered, and the reads
		// complete whether or not it was.
		std::vector<std::uint64_t> tags;
		bool answered = false;
		while (!answered || tags.size() < buffers.size())
		{
			const auto completion = port->wait();
			if (completion)
			{
				CHECK_FALSE( completion->get_error() );
				CHECK( completion->get_size() == 100 );
				tags.push_back(completion->get_tag());
			}
			else
			{
				answered = true;
			}
		}

		CHECK( tags.size() == buffers.size() );
		const auto bytes = file.get_bytes();
		for (std::uint64_t tag = 0; tag < buffers.size(); ++tag)
		{
			const auto first = bytes.begin() + 1000 * tag;
			CHECK( buffers[tag] == std::vector<byte>(first, first + 100) );
		}
	}

	SECTION( "wakes from several threads at once are answered" )
	{
		std::vector<std::thread> wakers;
		for (std::size_t index = 0; index < 4; ++index)
		{
			wakers.emplace_back(
				[&]
				{
					for (std::size_t count = 0; count < 100; ++count)
					{
						port->wake();
					}
				}
			);
		}

		for (auto &waker : wakers)
		{
			waker.join();
		}
		CHECK_FALSE( port->wait().has_value() );
	}
}

} // namespace vitrio
