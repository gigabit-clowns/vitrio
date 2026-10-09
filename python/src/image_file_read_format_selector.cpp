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
		"ImageFileReadFormatSelector"
	)
		.def_static(
			"get_shared",
			&image_file_read_format_selector::get_shared
		);
}

} // namespace vitrio
