// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_scope.hpp>

#include <vitrio/concurrency/task.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <trompeloeil.hpp>

#include <cstdint>
#include <memory>

namespace vitrio
{

class mock_file_transfer_scope final
	: public file_transfer_scope
{
public:
	mock_file_transfer_scope() = default;

	MAKE_MOCK4(
		queue_read,
		void(
			file_handle_t file,
			std::uint64_t offset,
			span<const memory_range> buffers,
			std::uint64_t tag
		),
		override
	);

	MAKE_MOCK4(
		queue_write,
		void(
			file_handle_t file,
			std::uint64_t offset,
			span<const const_memory_range> buffers,
			std::uint64_t tag
		),
		override
	);

	MAKE_MOCK2(
		queue_sync,
		void(file_handle_t file, std::uint64_t tag),
		override
	);

	MAKE_MOCK2(
		run,
		void(std::unique_ptr<task> work, std::uint64_t tag),
		override
	);
};

} // namespace vitrio
