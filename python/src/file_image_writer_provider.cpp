// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_image_writer_provider.hpp"

#include "utf8_string.hpp"

#include <vitrio/file_image_writer_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_writer_provider.hpp>

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/shared_ptr.h>

#include <filesystem>
#include <memory>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

void declare(
	file_image_writer_provider &self,
	const std::filesystem::path &key,
	const image_descriptor &descriptor
)
{
	self.declare(to_utf8_string(key), descriptor, image_metadata());
}

void close(file_image_writer_provider &self, const std::filesystem::path &key)
{
	const auto text = to_utf8_string(key);

	const nb::gil_scoped_release release;
	self.close(text);
}

} // anonymous namespace

void bind_file_image_writer_provider(nb::module_ &m)
{
	nb::class_<file_image_writer_provider, image_writer_provider>(
		m,
		"FileImageWriterProvider"
	)
		.def(
			nb::init<
				std::shared_ptr<const image_file_write_format_selector>
			>(),
			nb::arg("formats")
		)
		.def("declare", &declare, nb::arg("key"), nb::arg("descriptor"))
		.def("close", &close, nb::arg("key"))
		.def(
			"flush", &file_image_writer_provider::flush,
			nb::call_guard<nb::gil_scoped_release>()
		)
		.def_prop_ro(
			"file_count",
			&file_image_writer_provider::get_file_count
		);
}

} // namespace vitrio
