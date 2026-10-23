// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/file_transfer_inbox.hpp>
#include <file_transfer/file_transfer_task_notifier.hpp>

#include "../mock/mock_file_read_port.hpp"

#include <exception>
#include <memory>
#include <stdexcept>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a file_transfer_task_notifier posts the outcome of its task under its "
	"route",
	"[file_transfer_task_notifier]"
)
{
	mock_file_read_port port;
	ALLOW_CALL(port, wake());
	const auto inbox = std::make_shared<file_transfer_inbox>(port);

	const auto error = std::make_exception_ptr(std::runtime_error("task"));
	file_transfer_task_notifier(inbox, 3).notify_done();
	file_transfer_task_notifier(inbox, 5).notify_done(error);

	const auto mail = inbox->take();
	REQUIRE( mail.task_outcomes.size() == 2 );
	CHECK( mail.task_outcomes[0].route == 3 );
	CHECK_FALSE( mail.task_outcomes[0].error );
	CHECK( mail.task_outcomes[1].route == 5 );
	CHECK( mail.task_outcomes[1].error == error );
}

TEST_CASE(
	"a file_transfer_task_notifier keeps its inbox alive",
	"[file_transfer_task_notifier]"
)
{
	mock_file_read_port port;
	ALLOW_CALL(port, wake());
	auto inbox = std::make_shared<file_transfer_inbox>(port);
	const std::weak_ptr<file_transfer_inbox> watched = inbox;

	auto notifier = std::make_shared<file_transfer_task_notifier>(inbox, 0);
	inbox.reset();
	CHECK_FALSE( watched.expired() );

	notifier.reset();
	CHECK( watched.expired() );
}
