// SPDX-License-Identifier: LGPL-2.1-or-later

#include "executor.hpp"

#include <vitrio/concurrency/executor.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_executor(nb::module_ &m)
{
	nb::class_<executor>(
		m, "Executor",
		"Where the work of a loader or a saver runs.\n"
		"\n"
		"Its subclasses are `SynchronousExecutor`, which runs it in the "
		"calling thread, and `ThreadPoolExecutor`."
	);
}

} // namespace vitrio
