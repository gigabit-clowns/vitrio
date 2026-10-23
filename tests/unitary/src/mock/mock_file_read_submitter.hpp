// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_read_submitter.hpp>

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <trompeloeil.hpp>

#include <cstdint>

namespace vitrio
{

class mock_file_read_submitter final
	: public file_read_submitter
{
public:
	mock_file_read_submitter() = default;

	MAKE_MOCK4(
		queue,
		void(
			file_handle_t file,
			std::uint64_t offset,
			span<const memory_range> buffers,
			std::uint64_t tag
		),
		override
	);
};

} // namespace vitrio
