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
		"CachingImageReaderProvider"
	)
		.def(
			nb::init<std::shared_ptr<image_reader_provider>, std::size_t>(),
			nb::arg("backing"), nb::arg("capacity")
		)
		.def_prop_ro(
			"capacity",
			&caching_image_reader_provider::get_capacity
		)
		.def_prop_ro(
			"reader_count",
			&caching_image_reader_provider::get_reader_count
		);
}

} // namespace vitrio
