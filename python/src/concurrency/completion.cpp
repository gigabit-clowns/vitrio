// SPDX-License-Identifier: LGPL-2.1-or-later

#include "completion.hpp"

#include <vitrio/concurrency/completion.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_completion(nb::module_ &m)
{
	nb::class_<completion>(m, "Completion")
		.def(
			"wait", &completion::wait,
			nb::call_guard<nb::gil_scoped_release>()
		)
		.def(
			"get", &completion::get,
			nb::call_guard<nb::gil_scoped_release>()
		)
		.def_prop_ro("is_ready", &completion::is_ready);
}

} // namespace vitrio
