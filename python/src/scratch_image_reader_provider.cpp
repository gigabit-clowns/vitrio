// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scratch_image_reader_provider.hpp"

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
		"ScratchImageReaderProvider"
	)
		.def(
			nb::init<
				std::shared_ptr<image_reader_provider>,
				std::shared_ptr<image_scratch>
			>(),
			nb::arg("backing"), nb::arg("scratch")
		);
}

} // namespace vitrio
