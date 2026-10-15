// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_file_write_format_selector.hpp"

#include <vitrio/image_file_write_format_selector.hpp>

#include <nanobind/stl/shared_ptr.h>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_file_write_format_selector(nb::module_ &m)
{
	nb::class_<image_file_write_format_selector>(
		m,
		"ImageFileWriteFormatSelector",
		"Holds the formats a file may be written with.\n"
		"\n"
		"It creates a file with the one its path names."
	)
		.def_static(
			"get_shared",
			&image_file_write_format_selector::get_shared,
			"Get the selector that holds the formats that come with vitrio.\n"
			"\n"
			"Returns:\n"
			"    ImageFileWriteFormatSelector: MRC and, when built with TIFF, "
			"TIFF."
		);
}

} // namespace vitrio
