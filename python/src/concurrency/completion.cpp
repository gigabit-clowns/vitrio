// SPDX-License-Identifier: LGPL-2.1-or-later

#include "completion.hpp"

#include <vitrio/concurrency/completion.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_completion(nb::module_ &m)
{
	nb::class_<completion>(
		m, "Completion",
		"What an asynchronous read or write returns to wait on.\n"
		"\n"
		"It is ready once all the work behind it has run, successfully or "
		"not. Call `get` to wait for it and to learn whether anything failed. "
		"The completion keeps the arrays the work reads or writes alive, and "
		"destroying one that is not ready waits for it."
	)
		.def(
			"wait", &completion::wait,
			nb::call_guard<nb::gil_scoped_release>(),
			"Block until the work is done.\n"
			"\n"
			"It does not raise whatever failed. The GIL is released "
			"meanwhile."
		)
		.def(
			"get", &completion::get,
			nb::call_guard<nb::gil_scoped_release>(),
			"Block until the work is done, then raise the first failure.\n"
			"\n"
			"The GIL is released meanwhile.\n"
			"\n"
			"Raises:\n"
			"    Exception: Whatever the first piece of work to fail raised."
		)
		.def_prop_ro(
			"is_ready", &completion::is_ready,
			"Whether the work is done, successfully or not."
		);
}

} // namespace vitrio
