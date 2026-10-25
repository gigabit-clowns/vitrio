// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_image_writer_provider.hpp"

#include "utf8_string.hpp"

#include <vitrio/file_image_writer_provider.hpp>
#include <vitrio/file_transfer_pool.hpp>
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
		"FileImageWriterProvider",
		"Writes files, which are declared and closed explicitly.\n"
		"\n"
		"Creating a file replaces what was there, so a writer that was "
		"dropped and reopened would truncate what had been written. Each file "
		"therefore has one lifecycle: `declare` it, write to it as often as "
		"needed, and `close` it. A file is created the first time it is "
		"written to, and closing it flushes it and gives its file descriptor "
		"back."
	)
		.def(
			nb::init<
				std::shared_ptr<const image_file_write_format_selector>,
				std::shared_ptr<const file_transfer_pool>
			>(),
			nb::arg("formats"),
			nb::arg("transfers"),
			"Make a provider with no file declared.\n"
			"\n"
			"Args:\n"
			"    formats: The `ImageFileWriteFormatSelector` that creates the "
			"files.\n"
			"    transfers: The `FileTransferPool` whose threads carry out the "
			"writes."
		)
		.def(
			"declare", &declare, nb::arg("key"), nb::arg("descriptor"),
			"Make a file writable.\n"
			"\n"
			"Nothing is created until the file is first written to.\n"
			"\n"
			"Args:\n"
			"    key: The path of the file.\n"
			"    descriptor: The `ImageDescriptor` the file is created with.\n"
			"\n"
			"Raises:\n"
			"    RuntimeError: If the file is still declared. Close it first."
		)
		.def(
			"close", &close, nb::arg("key"),
			"Finish a file.\n"
			"\n"
			"It is flushed and its writer dropped. A closed file can be "
			"declared again.\n"
			"\n"
			"Args:\n"
			"    key: The path of the file.\n"
			"\n"
			"Raises:\n"
			"    IndexError: If the file is not declared."
		)
		.def(
			"flush", &file_image_writer_provider::flush,
			nb::call_guard<nb::gil_scoped_release>(),
			"Make what has been written to the open files reach the storage.\n"
			"\n"
			"The files stay declared and writable. The GIL is released "
			"meanwhile."
		)
		.def_prop_ro(
			"file_count",
			&file_image_writer_provider::get_file_count,
			"How many files are declared."
		);
}

} // namespace vitrio
