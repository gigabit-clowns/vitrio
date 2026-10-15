// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_write.hpp"

#include "utf8_string.hpp"

#include <array/array_import.hpp>
#include <array/host_ndarray.hpp>
#include <array/memory_loan.hpp>
#include <concurrency/memory_loan_completion.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_saver.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/span.hpp>

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/vector.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

void py_write_single(
	const_host_ndarray source,
	const std::filesystem::path &path,
	const image_file_write_format_selector &formats,
	std::optional<numerical_type> data_type
)
{
	const memory_loan loan(source);
	const auto imported = import_const_array(source, loan);
	const auto text = to_utf8_string(path);

	const nb::gil_scoped_release release;
	write_single(
		const_array_ref(imported),
		text,
		formats,
		data_type.value_or(numerical_type::unknown)
	);
}

void py_write_stack(
	const_host_ndarray source,
	const std::filesystem::path &path,
	const image_file_write_format_selector &formats,
	std::optional<numerical_type> data_type
)
{
	const memory_loan loan(source);
	const auto imported = import_const_array(source, loan);
	const auto text = to_utf8_string(path);

	const nb::gil_scoped_release release;
	write_stack(
		const_array_ref(imported),
		text,
		formats,
		data_type.value_or(numerical_type::unknown)
	);
}

void py_write(
	const_host_ndarray source,
	const std::filesystem::path &path,
	const image_file_write_format_selector &formats,
	const image_descriptor &descriptor
)
{
	const memory_loan loan(source);
	const auto imported = import_const_array(source, loan);
	const auto text = to_utf8_string(path);

	const nb::gil_scoped_release release;
	write(const_array_ref(imported), text, formats, descriptor);
}

std::shared_ptr<completion> py_write_batch_async(
	const image_saver &saver,
	const_host_ndarray source,
	const std::vector<image_location> &locations
)
{
	memory_loan loan(source);
	auto imported = import_const_array(source, loan);

	std::shared_ptr<completion> work;
	{
		const nb::gil_scoped_release release;
		work = write_batch_async(
			saver,
			std::move(imported),
			make_span(locations)
		);
	}

	return std::make_shared<memory_loan_completion>(
		std::move(work),
		std::move(loan)
	);
}

} // anonymous namespace

void bind_image_write(nb::module_ &m)
{
	m.def(
		"write_single", &py_write_single,
		nb::arg("array").noconvert(), nb::arg("path"), nb::arg("formats"),
		nb::arg("data_type") = nb::none(),
		"Write an array to a file as one image or volume.\n"
		"\n"
		"Prefer `vitrio.write_single`, which defaults the formats.\n"
		"\n"
		"Args:\n"
		"    array: The values, as any host array, read from where it is.\n"
		"    path: The file to create.\n"
		"    formats: The `ImageFileWriteFormatSelector` that creates it.\n"
		"    data_type: The type to store the values as. Defaults to the one "
		"the array carries."
	);
	m.def(
		"write_stack", &py_write_stack,
		nb::arg("array").noconvert(), nb::arg("path"), nb::arg("formats"),
		nb::arg("data_type") = nb::none(),
		"Write an array to a file as a stack of images or volumes.\n"
		"\n"
		"The leading extent of the array is the axis the file stacks along. "
		"Prefer `vitrio.write_stack`, which defaults the formats.\n"
		"\n"
		"Args:\n"
		"    array: The values, as any host array, read from where it is.\n"
		"    path: The file to create.\n"
		"    formats: The `ImageFileWriteFormatSelector` that creates it.\n"
		"    data_type: The type to store the values as. Defaults to the one "
		"the array carries."
	);
	m.def(
		"write", &py_write,
		nb::arg("array").noconvert(), nb::arg("path"), nb::arg("formats"),
		nb::arg("descriptor"),
		"Write an array to a file as a descriptor states.\n"
		"\n"
		"Prefer `vitrio.write`, which defaults the formats.\n"
		"\n"
		"Args:\n"
		"    array: The values, as any host array. Its shape has to be the "
		"extents of `descriptor`.\n"
		"    path: The file to create.\n"
		"    formats: The `ImageFileWriteFormatSelector` that creates it.\n"
		"    descriptor: The `ImageDescriptor` of what the file holds."
	);
	m.def(
		"write_batch_async", &py_write_batch_async,
		nb::arg("saver"), nb::arg("source").noconvert(),
		nb::arg("locations"),
		"Write one image or volume per location out of a batch.\n"
		"\n"
		"Returns before the writes are done. Slot `i` of `source` goes where "
		"`locations[i]` names. The files have to be declared with their final "
		"shape beforehand, on the provider the saver writes through, so a "
		"stack can be written a batch at a time, and one batch may span "
		"several stacks.\n"
		"\n"
		"Args:\n"
		"    saver: The `ImageSaver` the writes are dispatched to.\n"
		"    source: The values, as any host array, read from where it is. "
		"Its leading extent is the batch size and the rest are the shape of "
		"one image.\n"
		"    locations: A list of `ImageLocation`, one for each slot, in "
		"order.\n"
		"\n"
		"Returns:\n"
		"    Completion: Ready once every image is written or has failed. It "
		"keeps `source` alive, and destroying it waits for the writes. Its "
		"`get` raises the first failure.\n"
		"\n"
		"Raises:\n"
		"    ValueError: If the leading extent of `source` is not the number "
		"of locations, or if the locations mix those that name an index with "
		"those that name none."
	);
}

} // namespace vitrio
