// SPDX-License-Identifier: LGPL-2.1-or-later

#include "executor.hpp"

#include <vitrio/concurrency/executor.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_executor(nb::module_ &m)
{
	nb::class_<executor>(m, "Executor");
}

} // namespace vitrio
