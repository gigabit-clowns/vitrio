// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_writer_provider.hpp"

#include <vitrio/image_writer_provider.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_writer_provider(nb::module_ &m)
{
	nb::class_<image_writer_provider>(
		m, "ImageWriterProvider",
		"Turns a key into an open writer of images.\n"
		"\n"
		"`FileImageWriterProvider`, whose keys are paths, is the one that "
		"comes with vitrio."
	);
}

} // namespace vitrio
