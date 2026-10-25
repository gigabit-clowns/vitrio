// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_file_read_format_selector.hpp"

#include <vitrio/image_file_read_format_selector.hpp>

#include <nanobind/stl/shared_ptr.h>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_file_read_format_selector(nb::module_ &m)
{
	nb::class_<image_file_read_format_selector>(
		m,
		"ImageFileReadFormatSelector",
		"Holds the formats a file may be read with.\n"
		"\n"
		"It opens a file with the one that suits it.\n"
		"\n"
		"A file is recognized by its contents where its format allows it, and "
		"by its extension otherwise."
	)
		.def_static(
			"get_shared",
			&image_file_read_format_selector::get_shared,
			"Get the selector that holds the formats that come with vitrio.\n"
			"\n"
			"Returns:\n"
			"    ImageFileReadFormatSelector: MRC, TIFF and EER (when built "
			"with TIFF), DM and ZARR (when built with ZARR)."
		);
}

} // namespace vitrio
