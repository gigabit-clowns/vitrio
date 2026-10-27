// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_image_reader_provider.hpp"

#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_file_image_reader_provider(nb::module_ &m)
{
	nb::class_<file_image_reader_provider, image_reader_provider>(
		m,
		"FileImageReaderProvider",
		"Opens the file at a path with the format that suits it."
	)
		.def(
			nb::init<
				std::shared_ptr<const image_file_read_format_selector>,
				std::shared_ptr<const file_transfer_pool>
			>(),
			nb::arg("formats"),
			nb::arg("transfers"),
			"Make a provider.\n"
			"\n"
			"Args:\n"
			"    formats: The `ImageFileReadFormatSelector` that opens the "
			"files.\n"
			"    transfers: The `FileTransferPool` whose threads carry out the "
			"reads."
		);
}

} // namespace vitrio
