// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_reader_provider.hpp"

#include "utf8_string.hpp"

#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_reader_provider.hpp>

#include <nanobind/stl/filesystem.h>

#include <filesystem>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

image_descriptor py_query_descriptor(
	image_reader_provider &readers,
	const std::filesystem::path &key
)
{
	const auto text = to_utf8_string(key);

	const nb::gil_scoped_release release;
	return query_descriptor(readers, text);
}

} // anonymous namespace

void bind_image_reader_provider(nb::module_ &m)
{
	nb::class_<image_reader_provider>(
		m, "ImageReaderProvider",
		"Turns a key into an open reader of images.\n"
		"\n"
		"What a key means is up to the provider. The providers of files take "
		"paths, and `CachingImageReaderProvider` and "
		"`ScratchImageReaderProvider` stand in front of another provider. "
		"`vitrio.reader_provider` assembles them."
	);

	m.def(
		"query_descriptor", &py_query_descriptor,
		nb::arg("readers"), nb::arg("key"),
		"Get the descriptor of what a key holds, without reading it.\n"
		"\n"
		"Prefer `vitrio.query_descriptor`, which defaults the readers.\n"
		"\n"
		"Args:\n"
		"    readers: Where the key becomes a reader.\n"
		"    key: The key, a path for the providers of files.\n"
		"\n"
		"Returns:\n"
		"    ImageDescriptor: What the file holds."
	);
}

} // namespace vitrio
