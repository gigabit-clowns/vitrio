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
	nb::class_<image_reader_provider>(m, "ImageReaderProvider");

	m.def(
		"query_descriptor", &py_query_descriptor,
		nb::arg("readers"), nb::arg("key")
	);
}

} // namespace vitrio
