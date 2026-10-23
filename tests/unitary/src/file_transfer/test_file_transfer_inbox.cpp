// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/file_transfer_inbox.hpp>

#include "../mock/mock_completion_notifier.hpp"
#include "../mock/mock_file_read_port.hpp"
#include "../mock/mock_file_transfer_operation.hpp"

#include <exception>
#include <memory>
#include <stdexcept>

#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

file_transfer_inbox::admission make_admission()
{
	return file_transfer_inbox::admission{
		std::make_unique<mock_file_transfer_operation>(),
		std::make_shared<mock_completion_notifier>()
	};
}

} // anonymous namespace

TEST_CASE(
	"a file_transfer_inbox hands over what was posted, all at once",
	"[file_transfer_inbox]"
)
{
	mock_file_read_port port;
	ALLOW_CALL(port, wake());
	file_transfer_inbox inbox(port);

	auto first = make_admission();
	const auto *operation = first.operation.get();
	const auto error = std::make_exception_ptr(std::runtime_error("task"));

	inbox.post(std::move(first));
	inbox.post(make_admission());
	inbox.post(file_transfer_inbox::task_outcome{7, error});

	auto mail = inbox.take();
	REQUIRE( mail.admissions.size() == 2 );
	CHECK( mail.admissions[0].operation.get() == operation );
	REQUIRE( mail.task_outcomes.size() == 1 );
	CHECK( mail.task_outcomes[0].route == 7 );
	CHECK( mail.task_outcomes[0].error == error );
	CHECK_FALSE( mail.closed );
	CHECK_FALSE( mail.is_empty() );

	const auto rest = inbox.take();
	CHECK( rest.is_empty() );
	CHECK_FALSE( rest.closed );
}

TEST_CASE(
	"a file_transfer_inbox wakes its port when mail comes to it empty",
	"[file_transfer_inbox]"
)
{
	mock_file_read_port port;
	file_transfer_inbox inbox(port);

	SECTION( "an admission" )
	{
		REQUIRE_CALL(port, wake());
		inbox.post(make_admission());
	}

	SECTION( "the outcome of a task" )
	{
		REQUIRE_CALL(port, wake());
		inbox.post(file_transfer_inbox::task_outcome{0, nullptr});
	}

	SECTION( "mail after the earlier mail was taken" )
	{
		{
			REQUIRE_CALL(port, wake());
			inbox.post(make_admission());
		}
		inbox.take();

		REQUIRE_CALL(port, wake());
		inbox.post(make_admission());
	}
}

TEST_CASE(
	"a file_transfer_inbox does not wake its port again for mail that "
	"joins mail not taken yet",
	"[file_transfer_inbox]"
)
{
	mock_file_read_port port;
	file_transfer_inbox inbox(port);

	{
		REQUIRE_CALL(port, wake());
		inbox.post(make_admission());
	}

	FORBID_CALL(port, wake());
	inbox.post(make_admission());
	inbox.post(file_transfer_inbox::task_outcome{0, nullptr});
}

TEST_CASE(
	"a file_transfer_inbox that is closed says so, and wakes its port",
	"[file_transfer_inbox]"
)
{
	mock_file_read_port port;
	file_transfer_inbox inbox(port);

	{
		REQUIRE_CALL(port, wake());
		inbox.post(make_admission());
	}

	REQUIRE_CALL(port, wake());
	inbox.close();

	const auto mail = inbox.take();
	CHECK( mail.admissions.size() == 1 );
	CHECK( mail.closed );

	ALLOW_CALL(port, wake());
	inbox.post(file_transfer_inbox::task_outcome{0, nullptr});
	CHECK( inbox.take().closed );
}
