// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_dispatch.hpp"

#include "file_transfer_scope.hpp"
#include "file_transfer_task_notifier.hpp"

#include <assert.hpp>
#include <logger.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

// Reports to a notifier, which should not throw. What it throws anyway can be
// reported nowhere else, and the thread goes on.
void report(
	completion_notifier &done,
	std::exception_ptr failure
) noexcept
{
	try
	{
		done.notify_done(std::move(failure));
	}
	catch (const std::exception &error)
	{
		VITRIO_LOG_ERROR(
			"A notifier threw while a file transfer was reported to it: {}",
			error.what()
		);
	}
	catch (...)
	{
		VITRIO_LOG_ERROR(
			"A notifier threw while a file transfer was reported to it."
		);
	}
}

} // anonymous namespace

// The scope of the calls of one operation: what it queues and runs goes
// under routes that lead back to it.
class file_transfer_dispatch::operation_scope final
	: public file_transfer_scope
{
public:
	operation_scope(file_transfer_dispatch &dispatch, std::size_t slot) noexcept
		: m_dispatch(dispatch)
		, m_slot(slot)
	{
	}

	void queue_read(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override
	{
		const auto route = m_dispatch.open_route(m_slot, tag);
		try
		{
			m_dispatch.m_transfers.queue_read(file, offset, buffers, route);
		}
		catch (...)
		{
			m_dispatch.close_route(route);
			throw;
		}
	}

	void run(std::unique_ptr<task> work, std::uint64_t tag) override
	{
		if (!work)
		{
			throw std::invalid_argument(
				"file_transfer_scope: The task must not be null."
			);
		}

		const auto route = m_dispatch.open_route(m_slot, tag);
		try
		{
			m_dispatch.m_tasks.submit(
				std::move(work),
				std::make_shared<file_transfer_task_notifier>(
					m_dispatch.m_inbox,
					route
				)
			);
		}
		catch (...)
		{
			m_dispatch.close_route(route);
			throw;
		}
	}

private:
	file_transfer_dispatch &m_dispatch;
	std::size_t m_slot;
};

template <typename Call>
void file_transfer_dispatch::call(std::size_t slot, Call &&invoke) noexcept
{
	// The slots stay where they are during a call: an operation only opens
	// routes, and what it submits goes through the inbox.
	auto &called = m_slots[slot];
	if (called.failure)
	{
		return;
	}

	operation_scope scope(*this, slot);
	try
	{
		invoke(*called.operation, scope);
	}
	catch (...)
	{
		called.failure = std::current_exception();
	}
}

file_transfer_dispatch::file_transfer_dispatch(
	file_transfer_submitter &transfers,
	executor &tasks,
	std::shared_ptr<file_transfer_inbox> inbox
) noexcept
	: m_transfers(transfers)
	, m_tasks(tasks)
	, m_inbox(std::move(inbox))
{
	VITRIO_ASSERT(m_inbox);
}

file_transfer_dispatch::~file_transfer_dispatch()
{
	VITRIO_ASSERT(is_idle());
}

void file_transfer_dispatch::admit(
	std::unique_ptr<file_transfer_operation> operation,
	std::shared_ptr<completion_notifier> done
) noexcept
{
	VITRIO_ASSERT(operation);
	VITRIO_ASSERT(done);

	std::size_t index;
	try
	{
		index = take_slot();
	}
	catch (...)
	{
		// The operation is not started, so there is nothing to wait for.
		operation.reset();
		report(*done, std::current_exception());
		return;
	}

	auto &admitted = m_slots[index];
	admitted.operation = std::move(operation);
	admitted.done = std::move(done);

	call(
		index,
		[](file_transfer_operation &started, file_transfer_scope &scope)
		{
			started.start(scope);
		}
	);
	settle(index);
}

void file_transfer_dispatch::deliver_transfer(
	const file_transfer_completion &completion
) noexcept
{
	const auto target = close_route(completion.get_tag());

	call(
		target.slot,
		[&](file_transfer_operation &reading, file_transfer_scope &scope)
		{
			if (completion.get_error())
			{
				reading.on_transfer(
					file_transfer_completion(
						target.tag,
						completion.get_error()
					),
					scope
				);
			}
			else
			{
				reading.on_transfer(
					file_transfer_completion(
						target.tag,
						completion.get_size()
					),
					scope
				);
			}
		}
	);
	settle(target.slot);
}

void file_transfer_dispatch::deliver_task(
	std::uint64_t route,
	std::exception_ptr error
) noexcept
{
	const auto target = close_route(route);

	call(
		target.slot,
		[&](file_transfer_operation &running, file_transfer_scope &scope)
		{
			running.on_task(target.tag, std::move(error), scope);
		}
	);
	settle(target.slot);
}

bool file_transfer_dispatch::is_idle() const noexcept
{
	return m_free_slots.size() == m_slots.size();
}

std::size_t file_transfer_dispatch::take_slot()
{
	if (m_free_slots.empty())
	{
		// The free list gets room for every slot up front, so that freeing
		// one never fails.
		m_free_slots.reserve(m_slots.size() + 1);
		m_slots.push_back(slot{nullptr, nullptr, 0, nullptr});
		return m_slots.size() - 1;
	}

	const auto index = m_free_slots.back();
	m_free_slots.pop_back();
	return index;
}

std::uint64_t file_transfer_dispatch::open_route(
	std::size_t slot,
	std::uint64_t tag
)
{
	std::uint64_t index;
	if (m_free_routes.empty())
	{
		// As with the slots, freeing a route never fails.
		m_free_routes.reserve(m_routes.size() + 1);
		m_routes.push_back(route{slot, tag});
		index = m_routes.size() - 1;
	}
	else
	{
		index = m_free_routes.back();
		m_free_routes.pop_back();
		m_routes[index] = route{slot, tag};
	}

	++m_slots[slot].outstanding;
	return index;
}

file_transfer_dispatch::route file_transfer_dispatch::close_route(
	std::uint64_t index
) noexcept
{
	VITRIO_ASSERT(index < m_routes.size());

	const auto closed = m_routes[index];
	m_free_routes.push_back(index);

	VITRIO_ASSERT(m_slots[closed.slot].outstanding > 0);
	--m_slots[closed.slot].outstanding;
	return closed;
}

void file_transfer_dispatch::settle(std::size_t slot) noexcept
{
	auto &settled = m_slots[slot];
	if (settled.outstanding > 0)
	{
		return;
	}

	const auto done = std::move(settled.done);
	auto failure = std::move(settled.failure);
	settled.failure = nullptr;

	// The operation lets go of what it holds before the outcome is known.
	settled.operation.reset();
	m_free_slots.push_back(slot);

	report(*done, std::move(failure));
}

} // namespace vitrio
