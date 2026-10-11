// SPDX-License-Identifier: LGPL-2.1-or-later

#include "indexed_image_scratch.hpp"

#include "utf8_string.hpp"

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
	std::size_t run_length,
	std::optional<std::size_t> max_size
)
{
	const nb::gil_scoped_release release;
	return create_host_image_scratch(
		locations,
		files,
		run_length,
		to_max_size(max_size)
	);
}

std::shared_ptr<image_scratch> py_create_mapped_file_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
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
		text,
		run_length,
		to_max_size(max_size)
	);
}

std::shared_ptr<image_scratch> py_create_directory_image_scratch(
	const image_location_grouping &locations,
	image_reader_provider &files,
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
		nb::arg("locations"), nb::arg("files"), nb::arg("run_length"),
		nb::arg("max_size") = nb::none()
	);
	m.def(
		"create_mapped_file_image_scratch",
		&py_create_mapped_file_image_scratch,
		nb::arg("locations"), nb::arg("files"), nb::arg("path"),
		nb::arg("run_length"), nb::arg("max_size") = nb::none()
	);
	m.def(
		"create_directory_image_scratch",
		&py_create_directory_image_scratch,
		nb::arg("locations"), nb::arg("files"), nb::arg("directory"),
		nb::arg("run_length"), nb::arg("budget")
	);
}

} // namespace vitrio
