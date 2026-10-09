// SPDX-License-Identifier: LGPL-2.1-or-later

#include "synchronous_executor.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/synchronous_executor.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_synchronous_executor(nb::module_ &m)
{
	nb::class_<synchronous_executor, executor>(m, "SynchronousExecutor")
		.def(nb::init<>());
}

} // namespace vitrio
