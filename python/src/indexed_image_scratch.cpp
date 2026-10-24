// SPDX-License-Identifier: LGPL-2.1-or-later

#include "indexed_image_scratch.hpp"

#include "utf8_string.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch.hpp>
#include <vitrio/indexed_image_scratch.hpp>

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

std::size_t to_max_size(std::optional<std::size_t> max_size)
{
	return max_size.value_or(std::numeric_limits<std::size_t>::max());
}

std::shared_ptr<image_scratch> py_create_host_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	vitrio::executor &executor,
	std::size_t run_length,
	std::optional<std::size_t> max_size
)
{
	const nb::gil_scoped_release release;
	return create_host_image_scratch(
		locations,
		files,
		executor,
		run_length,
		to_max_size(max_size)
	);
}

std::shared_ptr<image_scratch> py_create_mapped_file_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	vitrio::executor &executor,
	const std::filesystem::path &path,
	std::size_t run_length,
	std::optional<std::size_t> max_size
)
{
	const auto text = to_utf8_string(path);

	const nb::gil_scoped_release release;
	return create_mapped_file_image_scratch(
		locations,
		files,
		executor,
		text,
		run_length,
		to_max_size(max_size)
	);
}

std::shared_ptr<image_scratch> py_create_directory_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
	vitrio::executor &executor,
	const std::filesystem::path &directory,
	std::size_t run_length,
	std::size_t budget
)
{
	const auto text = to_utf8_string(directory);

	const nb::gil_scoped_release release;
	return create_directory_image_scratch(
		locations,
		files,
		executor,
		text,
		run_length,
		budget
	);
}

} // anonymous namespace

void bind_indexed_image_scratch(nb::module_ &m)
{
	m.def(
		"create_host_image_scratch", &py_create_host_image_scratch,
		nb::arg("locations"), nb::arg("files"), nb::arg("executor"),
		nb::arg("run_length"), nb::arg("max_size") = nb::none(),
		"Create a scratch that keeps its copies in main memory.\n"
		"\n"
		"Prefer `vitrio.create_host_image_scratch`, which defaults the files "
		"and the executor.\n"
		"\n"
		"Args:\n"
		"    locations: An `ImageLocationGrouping` of the images to hold.\n"
		"    files: The `ImageReaderProvider` that reads the files.\n"
		"    executor: Where the files are opened, all at once. This waits "
		"for them, so it must not be called on a thread of it.\n"
		"    run_length: How many held images of a file are taken in "
		"together.\n"
		"    max_size: The most bytes to allocate. Defaults to no maximum.\n"
		"\n"
		"Returns:\n"
		"    ImageScratch: The scratch, which starts empty."
	);
	m.def(
		"create_mapped_file_image_scratch",
		&py_create_mapped_file_image_scratch,
		nb::arg("locations"), nb::arg("files"), nb::arg("executor"),
		nb::arg("path"), nb::arg("run_length"),
		nb::arg("max_size") = nb::none(),
		"Create a scratch that keeps its copies in a file mapped into memory.\n"
		"\n"
		"Prefer `vitrio.create_mapped_file_image_scratch`, which defaults the "
		"files and the executor.\n"
		"\n"
		"Args:\n"
		"    locations: An `ImageLocationGrouping` of the images to hold.\n"
		"    files: The `ImageReaderProvider` that reads the files.\n"
		"    executor: Where the files are opened, all at once. This waits "
		"for them, so it must not be called on a thread of it.\n"
		"    path: The file that holds the copies. It is not removed.\n"
		"    run_length: How many held images of a file are taken in "
		"together.\n"
		"    max_size: The most bytes to map. Defaults to no maximum.\n"
		"\n"
		"Returns:\n"
		"    ImageScratch: The scratch, which resumes from what the file "
		"holds."
	);
	m.def(
		"create_directory_image_scratch",
		&py_create_directory_image_scratch,
		nb::arg("locations"), nb::arg("files"), nb::arg("executor"),
		nb::arg("directory"), nb::arg("run_length"), nb::arg("budget"),
		"Create a scratch that keeps its files in a directory, to a budget.\n"
		"\n"
		"Prefer `vitrio.create_directory_image_scratch`, which defaults the "
		"files and the executor.\n"
		"\n"
		"Args:\n"
		"    locations: An `ImageLocationGrouping` of the images to hold.\n"
		"    files: The `ImageReaderProvider` that reads the files.\n"
		"    executor: Where the files are opened, all at once. This waits "
		"for them, so it must not be called on a thread of it.\n"
		"    directory: Where the files are kept.\n"
		"    run_length: How many held images of a file are taken in "
		"together.\n"
		"    budget: The most bytes the files of the directory may take. The "
		"files used least recently are removed to make room.\n"
		"\n"
		"Returns:\n"
		"    ImageScratch: The scratch."
	);
}

} // namespace vitrio
