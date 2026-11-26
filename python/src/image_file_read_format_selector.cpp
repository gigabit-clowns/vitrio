// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_file_read_format_selector.hpp"

#include "image_metadata.hpp"
#include "utf8_string.hpp"

#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_metadata.hpp>

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/shared_ptr.h>

#include <filesystem>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

nb::dict read_metadata(
	const image_file_read_format_selector &self,
	const std::filesystem::path &path
)
{
	const auto text = to_utf8_string(path);

	image_metadata metadata;
	{
		const nb::gil_scoped_release release;
		metadata = self.read_metadata(text);
	}

	return export_image_metadata(metadata);
}

} // anonymous namespace

void bind_image_file_read_format_selector(nb::module_ &m)
{
	nb::class_<image_file_read_format_selector>(
		m,
		"ImageFileReadFormatSelector",
		"Holds the formats a file may be read with.\n"
		"\n"
		"It opens a file, or reads its metadata, with the one that suits "
		"it.\n"
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
		)
		.def(
			"read_metadata", &read_metadata, nb::arg("path"),
			"Read what a file states beyond its shape and data type.\n"
			"\n"
			"Prefer `vitrio.read_metadata`, which defaults the formats and "
			"says what the labels mean. The GIL is released while the file "
			"is read.\n"
			"\n"
			"Args:\n"
			"    path: The file to read.\n"
			"\n"
			"Returns:\n"
			"    dict: An `int`, a `float` or a `str` under each label the "
			"file states and its format reads, sorted by label."
		);
}

} // namespace vitrio
