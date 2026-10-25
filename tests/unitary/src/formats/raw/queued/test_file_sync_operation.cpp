// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/file_sync_operation.hpp>

#include "../../../mock/mock_file_transfer_scope.hpp"

#include <formats/raw/create_image_file.hpp>
#include <formats/raw/queued/writable_image_file.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <cstddef>
#include <memory>
#include <system_error>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a file_sync_operation syncs its file as it starts, and fails with a "
	"sync the system fails",
	"[file_sync_operation]"
)
{
	const scoped_path path("file_sync_operation.raw");
	create_image_file(path.get(), 64);
	const auto file = std::make_shared<writable_image_file>(path.get());
	file_sync_operation operation(file);

	mock_file_transfer_scope scope;
	REQUIRE_CALL(scope, queue_sync(file->get_file(), 0u));
	operation.start(scope);

	SECTION( "a sync that succeeds is done" )
	{
		CHECK_NOTHROW(
			operation.on_transfer(file_transfer_completion(0, 0), scope)
		);
	}

	SECTION( "a sync the system fails fails it, naming the file" )
	{
		REQUIRE_THROWS_MATCHES(
			operation.on_transfer(
				file_transfer_completion(
					0,
					std::make_error_code(std::errc::io_error)
				),
				scope
			),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get() + ": ")
			)
		);
	}
}
