// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_operation.hpp>
#include <file_transfer/file_transfer_scope.hpp>

#include <system/file_transfer_completion.hpp>

#include <trompeloeil.hpp>

#include <cstdint>
#include <exception>
#include <memory>
#include <vector>

namespace vitrio
{

// Not final, so that a case can watch it be destroyed, with
// trompeloeil::deathwatched.
class mock_file_transfer_operation
	: public file_transfer_operation
{
public:
	mock_file_transfer_operation() = default;

	MAKE_MOCK1(start, void(file_transfer_scope &scope), override);

	MAKE_MOCK2(
		on_transfer,
		void(
			const file_transfer_completion &completion,
			file_transfer_scope &scope
		),
		override
	);

	MAKE_MOCK3(
		on_task,
		void(
			std::uint64_t tag,
			std::exception_ptr error,
			file_transfer_scope &scope
		),
		override
	);

	// The code under test destroys the operation, so its expectations go
	// with it, and are checked then.
	std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
};

} // namespace vitrio
