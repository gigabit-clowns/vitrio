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
		nb::arg("data_type") = nb::none()
	);
	m.def(
		"write_stack", &py_write_stack,
		nb::arg("array").noconvert(), nb::arg("path"), nb::arg("formats"),
		nb::arg("data_type") = nb::none()
	);
	m.def(
		"write", &py_write,
		nb::arg("array").noconvert(), nb::arg("path"), nb::arg("formats"),
		nb::arg("descriptor")
	);
	m.def(
		"write_batch_async", &py_write_batch_async,
		nb::arg("saver"), nb::arg("source").noconvert(),
		nb::arg("locations")
	);
}

} // namespace vitrio
