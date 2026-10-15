// SPDX-License-Identifier: LGPL-2.1-or-later

#include "caching_image_reader_provider.hpp"

#include <vitrio/caching_image_reader_provider.hpp>
#include <vitrio/image_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <cstddef>
#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_caching_image_reader_provider(nb::module_ &m)
{
	nb::class_<caching_image_reader_provider, image_reader_provider>(
		m,
		"CachingImageReaderProvider",
		"Keeps the readers last asked for open, in front of a provider.\n"
		"\n"
		"Going back to a file then does not open it again. When it holds more "
		"readers than its capacity, the one used least recently is closed."
	)
		.def(
			nb::init<std::shared_ptr<image_reader_provider>, std::size_t>(),
			nb::arg("backing"), nb::arg("capacity"),
			"Put a cache in front of a provider.\n"
			"\n"
			"Args:\n"
			"    backing: The `ImageReaderProvider` that opens what is not "
			"cached.\n"
			"    capacity: How many readers to keep open at once."
		)
		.def_prop_ro(
			"capacity",
			&caching_image_reader_provider::get_capacity,
			"How many readers it may keep open at once."
		)
		.def_prop_ro(
			"reader_count",
			&caching_image_reader_provider::get_reader_count,
			"How many readers it keeps open now."
		);
}

} // namespace vitrio
