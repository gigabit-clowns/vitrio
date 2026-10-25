// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_executor.hpp>
#include <file_transfer/file_transfer_operation.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>

#include <memory>
#include <trompeloeil.hpp>

namespace vitrio
{

class mock_file_transfer_executor final
	: public file_transfer_executor
{
public:
	mock_file_transfer_executor() = default;

	MAKE_MOCK2(
		submit,
		void(
			std::unique_ptr<file_transfer_operation> operation,
			std::shared_ptr<completion_notifier> done
		),
		override
	);
};

} // namespace vitrio
