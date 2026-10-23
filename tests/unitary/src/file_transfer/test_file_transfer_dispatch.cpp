// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/file_transfer_dispatch.hpp>
#include <file_transfer/file_transfer_inbox.hpp>

#include "../fixtures/captured_log.hpp"
#include "../mock/mock_completion_notifier.hpp"
#include "../mock/mock_executor.hpp"
#include "../mock/mock_file_read_port.hpp"
#include "../mock/mock_file_read_submitter.hpp"
#include "../mock/mock_file_transfer_operation.hpp"
#include "../mock/mock_task.hpp"

#include <system/file_read_completion.hpp>

#include <vitrio/concurrency/counting_completion.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

span<const memory_range> no_buffers()
{
	return span<const memory_range>();
}

// A dispatch whose reads go to a mocked submitter and whose tasks go to a
// mocked executor, which a case completes by hand.
class dispatch_fixture
{
public:
	dispatch_fixture()
		: m_wakes(NAMED_ALLOW_CALL(m_port, wake()))
		, m_inbox(std::make_shared<file_transfer_inbox>(m_port))
		, m_dispatch(m_reads, m_tasks, m_inbox)
	{
	}

protected:
	// Lets the submitter take every read, and keeps the route of each.
	std::unique_ptr<trompeloeil::expectation> take_reads()
	{
		return NAMED_ALLOW_CALL(
			m_reads,
			queue(
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_
			)
		).LR_SIDE_EFFECT( m_read_routes.push_back(_4) );
	}

	// Lets the executor take every task, and keeps the notifier of each.
	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		return NAMED_ALLOW_CALL(
			m_tasks,
			submit(trompeloeil::_, trompeloeil::_)
		).LR_SIDE_EFFECT( m_task_notifiers.push_back(_2) );
	}

	// Hands the outcomes the tasks posted back to the dispatch.
	void deliver_task_outcomes()
	{
		auto mail = m_inbox->take();
		for (auto &outcome : mail.task_outcomes)
		{
			m_dispatch.deliver_task(outcome.route, outcome.error);
		}
	}

	mock_file_read_port m_port;
	std::unique_ptr<trompeloeil::expectation> m_wakes;
	std::shared_ptr<file_transfer_inbox> m_inbox;
	mock_file_read_submitter m_reads;
	mock_executor m_tasks;
	std::vector<std::uint64_t> m_read_routes;
	std::vector<std::shared_ptr<completion_notifier>> m_task_notifiers;
	file_transfer_dispatch m_dispatch;
};

} // anonymous namespace

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch starts an operation and reports it once its "
	"reads have all been handed back",
	"[file_transfer_dispatch]"
)
{
	const auto reads = take_reads();
	auto operation = std::make_unique<mock_file_transfer_operation>();
	auto &started = *operation;
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
			.SIDE_EFFECT(
				_1.queue(file_handle_t(), 100, no_buffers(), 10);
				_1.queue(file_handle_t(), 200, no_buffers(), 11)
			)
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, on_read(trompeloeil::_, trompeloeil::_))
			.WITH( _1.get_tag() == 11 && _1.get_size() == 4 )
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, on_read(trompeloeil::_, trompeloeil::_))
			.WITH( _1.get_tag() == 10 && _1.get_size() == 3 )
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	REQUIRE( m_read_routes.size() == 2 );
	CHECK( m_read_routes[0] != m_read_routes[1] );
	CHECK_FALSE( done->is_ready() );
	CHECK_FALSE( m_dispatch.is_idle() );

	m_dispatch.deliver_read(file_read_completion(m_read_routes[1], 4));
	CHECK_FALSE( done->is_ready() );

	m_dispatch.deliver_read(file_read_completion(m_read_routes[0], 3));
	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch reports an operation that starts nothing at "
	"once",
	"[file_transfer_dispatch]"
)
{
	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch hands each completion to the operation and "
	"the tag that queued its read",
	"[file_transfer_dispatch]"
)
{
	const auto reads = take_reads();
	const std::error_code refused(5, std::system_category());

	// Both queue a read with the same tag.
	auto first = std::make_unique<mock_file_transfer_operation>();
	auto &first_started = *first;
	first_started.expectations.push_back(
		NAMED_REQUIRE_CALL(first_started, start(trompeloeil::_))
			.SIDE_EFFECT(
				_1.queue(file_handle_t(), 0, no_buffers(), 10);
				_1.queue(file_handle_t(), 0, no_buffers(), 11)
			)
	);
	first_started.expectations.push_back(
		NAMED_REQUIRE_CALL(
			first_started,
			on_read(trompeloeil::_, trompeloeil::_)
		)
			.WITH( _1.get_tag() == 11 && _1.get_error() == refused )
	);
	first_started.expectations.push_back(
		NAMED_REQUIRE_CALL(
			first_started,
			on_read(trompeloeil::_, trompeloeil::_)
		)
			.WITH( _1.get_tag() == 10 && _1.get_size() == 7 )
	);

	auto second = std::make_unique<mock_file_transfer_operation>();
	auto &second_started = *second;
	second_started.expectations.push_back(
		NAMED_REQUIRE_CALL(second_started, start(trompeloeil::_))
			.SIDE_EFFECT( _1.queue(file_handle_t(), 0, no_buffers(), 10) )
	);
	second_started.expectations.push_back(
		NAMED_REQUIRE_CALL(
			second_started,
			on_read(trompeloeil::_, trompeloeil::_)
		)
			.WITH( _1.get_tag() == 10 && _1.get_size() == 5 )
	);

	const auto first_done = std::make_shared<counting_completion>(1);
	const auto second_done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(first), first_done);
	m_dispatch.admit(std::move(second), second_done);
	REQUIRE( m_read_routes.size() == 3 );

	m_dispatch.deliver_read(file_read_completion(m_read_routes[2], 5));
	CHECK( second_done->is_ready() );
	CHECK_FALSE( first_done->is_ready() );

	m_dispatch.deliver_read(file_read_completion(m_read_routes[1], refused));
	m_dispatch.deliver_read(file_read_completion(m_read_routes[0], 7));
	CHECK( first_done->is_ready() );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch destroys an operation before it reports it",
	"[file_transfer_dispatch]"
)
{
	trompeloeil::sequence order;
	auto operation = std::make_unique<
		trompeloeil::deathwatched<mock_file_transfer_operation>
	>();
	auto &started = *operation;
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
	);

	const auto done = std::make_shared<mock_completion_notifier>();
	REQUIRE_DESTRUCTION(started).IN_SEQUENCE(order);
	REQUIRE_CALL(*done, notify_done(trompeloeil::_))
		.WITH( _1 == nullptr )
		.IN_SEQUENCE(order);

	m_dispatch.admit(std::move(operation), done);
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch runs the tasks of an operation on its "
	"executor, and hands their outcomes back with their tags",
	"[file_transfer_dispatch]"
)
{
	const auto tasks = take_tasks();
	const auto failure = std::make_exception_ptr(std::runtime_error("task"));

	auto operation = std::make_unique<mock_file_transfer_operation>();
	auto &started = *operation;
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
			.SIDE_EFFECT(
				_1.run(std::make_unique<mock_task>(), 5);
				_1.run(std::make_unique<mock_task>(), 6)
			)
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(
			started,
			on_task(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.WITH( _1 == 5 && _2 == nullptr )
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(
			started,
			on_task(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH( _1 == 6 && _2 == failure )
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	REQUIRE( m_task_notifiers.size() == 2 );

	m_task_notifiers[0]->notify_done();
	deliver_task_outcomes();
	CHECK_FALSE( done->is_ready() );

	m_task_notifiers[1]->notify_done(failure);
	deliver_task_outcomes();
	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch reports what fails an operation once what "
	"it started has completed, and calls it no more",
	"[file_transfer_dispatch]"
)
{
	const auto reads = take_reads();
	const auto tasks = take_tasks();

	auto operation = std::make_unique<mock_file_transfer_operation>();
	auto &started = *operation;
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
			.SIDE_EFFECT( _1.queue(file_handle_t(), 0, no_buffers(), 0) )
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, on_read(trompeloeil::_, trompeloeil::_))
			.SIDE_EFFECT(
				_2.queue(file_handle_t(), 0, no_buffers(), 1);
				_2.run(std::make_unique<mock_task>(), 2)
			)
			.THROW( std::runtime_error("on_read") )
	);
	started.expectations.push_back(
		NAMED_FORBID_CALL(
			started,
			on_task(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	m_dispatch.deliver_read(file_read_completion(m_read_routes[0], 1));
	REQUIRE( m_read_routes.size() == 2 );
	REQUIRE( m_task_notifiers.size() == 1 );
	CHECK_FALSE( done->is_ready() );

	m_task_notifiers[0]->notify_done();
	deliver_task_outcomes();
	CHECK_FALSE( done->is_ready() );

	m_dispatch.deliver_read(file_read_completion(m_read_routes[1], 1));
	REQUIRE( done->is_ready() );
	CHECK_THROWS_AS( done->get(), std::runtime_error );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch reports an operation that fails as it starts "
	"at once, if it started nothing",
	"[file_transfer_dispatch]"
)
{
	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
			.THROW( std::runtime_error("start") )
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	REQUIRE( done->is_ready() );
	CHECK_THROWS_AS( done->get(), std::runtime_error );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a read or a task that is refused is not outstanding",
	"[file_transfer_dispatch]"
)
{
	REQUIRE_CALL(
		m_reads,
		queue(trompeloeil::_, trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.THROW( std::system_error(11, std::system_category(), "queue") );
	REQUIRE_CALL(m_tasks, submit(trompeloeil::_, trompeloeil::_))
		.THROW( std::runtime_error("submit") );

	// The operation goes on without them, and so is done at once.
	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
			.SIDE_EFFECT(
				CHECK_THROWS_AS(
					_1.queue(file_handle_t(), 0, no_buffers(), 0),
					std::system_error
				);
				CHECK_THROWS_AS(
					_1.run(std::make_unique<mock_task>(), 0),
					std::runtime_error
				)
			)
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
	CHECK( m_dispatch.is_idle() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"the scope of a file_transfer_dispatch refuses a null task",
	"[file_transfer_dispatch]"
)
{
	FORBID_CALL(m_tasks, submit(trompeloeil::_, trompeloeil::_));

	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
			.SIDE_EFFECT(
				CHECK_THROWS_AS( _1.run(nullptr, 0), std::invalid_argument )
			)
	);

	const auto done = std::make_shared<counting_completion>(1);
	m_dispatch.admit(std::move(operation), done);
	CHECK( done->is_ready() );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch logs what a notifier throws, and goes on",
	"[file_transfer_dispatch]"
)
{
	const captured_log log;

	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
	);

	const auto done = std::make_shared<mock_completion_notifier>();
	REQUIRE_CALL(*done, notify_done(trompeloeil::_))
		.THROW( std::runtime_error("notifier") );

	m_dispatch.admit(std::move(operation), done);
	CHECK( m_dispatch.is_idle() );
	CHECK( log.get().find("notifier") != std::string::npos );
}

TEST_CASE_METHOD(
	dispatch_fixture,
	"a file_transfer_dispatch carries out many operations, whose reads "
	"complete in any order",
	"[file_transfer_dispatch]"
)
{
	const auto reads = take_reads();
	constexpr std::size_t operation_count = 50;
	constexpr std::uint64_t reads_per_operation = 4;

	// Two rounds, the second over the slots and routes the first freed.
	for (std::size_t round = 0; round < 2; ++round)
	{
		m_read_routes.clear();
		const auto done = std::make_shared<counting_completion>(
			operation_count
		);
		for (std::size_t index = 0; index < operation_count; ++index)
		{
			auto operation = std::make_unique<mock_file_transfer_operation>();
			auto &started = *operation;
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
					.SIDE_EFFECT(
						for (std::uint64_t tag = 0; tag < 4; ++tag)
						{
							_1.queue(file_handle_t(), 0, no_buffers(), tag);
						}
					)
			);
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(
					started,
					on_read(trompeloeil::_, trompeloeil::_)
				)
					.WITH( _1.get_tag() < 4 && _1.get_size() == _1.get_tag() )
					.TIMES(4)
			);
			m_dispatch.admit(std::move(operation), done);
		}

		REQUIRE(
			m_read_routes.size() == operation_count * reads_per_operation
		);

		// Each read completes with as many bytes as its tag, last first.
		for (std::size_t index = m_read_routes.size(); index > 0; --index)
		{
			const auto tag = (index - 1) % reads_per_operation;
			m_dispatch.deliver_read(
				file_read_completion(m_read_routes[index - 1], tag)
			);
		}

		REQUIRE( done->is_ready() );
		CHECK_NOTHROW( done->get() );
		CHECK( m_dispatch.is_idle() );
	}
}
