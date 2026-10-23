// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_pool.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/file_transfer_pool.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>
#include <utility>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

// Destroying a pool waits for the transfers under way, which takes as long
// as they have left. The thread that destroys it holds the GIL, and lets go
// of it meanwhile.
std::shared_ptr<file_transfer_pool> make_pool(std::shared_ptr<executor> tasks)
{
	auto pool = std::make_unique<file_transfer_pool>(std::move(tasks));
	return std::shared_ptr<file_transfer_pool>(
		pool.release(),
		[] (file_transfer_pool *released) noexcept
		{
			const nb::gil_scoped_release release;
			delete released;
		}
	);
}

} // anonymous namespace

void bind_file_transfer_pool(nb::module_ &m)
{
	nb::class_<file_transfer_pool>(
		m, "FileTransferPool",
		"Threads that carry out the reads of files, each many at once, and "
		"hand the conversions those need to an executor.\n"
		"\n"
		"Prefer `vitrio.reader_provider` and `vitrio.loader`, which share one "
		"across the process. The readers opened with a pool keep what they "
		"need of it, and the last of them, or the pool, waits for the reads "
		"under way as it goes."
	)
		.def(
			nb::new_(&make_pool), nb::arg("executor"),
			"Start the threads.\n"
			"\n"
			"Args:\n"
			"    executor: Where the conversions run. They never wait, but "
			"work that waits for reads of this pool must not occupy every "
			"thread of it."
		);
}

} // namespace vitrio
