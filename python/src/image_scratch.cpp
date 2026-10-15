// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_scratch.hpp"

#include <vitrio/image_scratch.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_scratch(nb::module_ &m)
{
	nb::class_<image_scratch>(
		m, "ImageScratch",
		"A copy of the images a program goes back to, in RAM or in a file.\n"
		"\n"
		"Make one with `create_host_image_scratch`, "
		"`create_mapped_file_image_scratch` or "
		"`create_directory_image_scratch`, and read through it with "
		"`vitrio.loader(scratch=...)` or "
		"`vitrio.reader_provider(scratch=...)`. What it holds is read from "
		"it, and what it does not yet hold is read from the file and taken "
		"in."
	);
}

} // namespace vitrio
