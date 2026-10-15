// SPDX-License-Identifier: LGPL-2.1-or-later

#include "synchronous_executor.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/synchronous_executor.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_synchronous_executor(nb::module_ &m)
{
	nb::class_<synchronous_executor, executor>(
		m, "SynchronousExecutor",
		"Runs the work in the thread that submits it, before the call "
		"returns.\n"
		"\n"
		"Reads and writes through it are asynchronous in name only. It suits "
		"tests and debugging."
	)
		.def(nb::init<>(), "Make an executor.");
}

} // namespace vitrio
