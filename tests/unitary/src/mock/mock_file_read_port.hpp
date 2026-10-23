// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_read_completion.hpp>
#include <system/file_read_port.hpp>

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <boost/optional.hpp>

#include <trompeloeil.hpp>

#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>

namespace vitrio
{

class mock_file_read_port final
	: public file_read_port
{
public:
	mock_file_read_port() = default;

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

	MAKE_MOCK0(wake, void(), noexcept override);

	MAKE_MOCK0(wait, boost::optional<file_read_completion>(), override);

	// The code under test may destroy the port, so its expectations may go
	// with it, and are checked then.
	std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
};

} // namespace vitrio

namespace trompeloeil
{

// What a wait returned, as a report of trompeloeil says it: Boost refuses to
// write an optional without the header that writes its value, and a
// completion is not written.
template <>
struct printer<boost::optional<vitrio::file_read_completion>>
{
	static void print(
		std::ostream &os,
		const boost::optional<vitrio::file_read_completion> &completion
	)
	{
		if (!completion)
		{
			os << "no completion";
			return;
		}

		os << "the completion of the read tagged " << completion->get_tag();
	}
};

} // namespace trompeloeil
