// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scratch_image_reader_provider.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch.hpp>
#include <vitrio/scratch_image_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_scratch_image_reader_provider(nb::module_ &m)
{
	nb::class_<scratch_image_reader_provider, image_reader_provider>(
		m,
		"ScratchImageReaderProvider",
		"Serves reads from a scratch, in front of another provider.\n"
		"\n"
		"What the scratch holds is read from it, and what it does not hold "
		"yet is read from the provider behind and taken in."
	)
		.def(
			nb::init<
				std::shared_ptr<image_reader_provider>,
				std::shared_ptr<image_scratch>,
				std::shared_ptr<executor>
			>(),
			nb::arg("backing"), nb::arg("scratch"), nb::arg("executor"),
			"Put a scratch in front of a provider.\n"
			"\n"
			"Args:\n"
			"    backing: The `ImageReaderProvider` that reads what is not "
			"held.\n"
			"    scratch: The `ImageScratch` to serve reads from.\n"
			"    executor: Where what the scratch takes in for a read is read "
			"out of it."
		);
}

} // namespace vitrio
