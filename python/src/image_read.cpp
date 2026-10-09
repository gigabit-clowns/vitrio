// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_read.hpp"

#include "utf8_string.hpp"

#include <array/array_import.hpp>
#include <array/host_ndarray.hpp>
#include <array/memory_loan.hpp>
#include <concurrency/memory_loan_completion.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/image_loader.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/index_table.hpp>
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

array py_read_key(
	const std::filesystem::path &key,
	image_reader_provider &readers,
	std::optional<numerical_type> data_type
)
{
	const auto text = to_utf8_string(key);

	const nb::gil_scoped_release release;
	return read(text, readers, data_type.value_or(numerical_type::unknown));
}

array py_read_location(
	const image_location &location,
	image_reader_provider &readers,
	std::optional<numerical_type> data_type
)
{
	const nb::gil_scoped_release release;
	return read(
		location,
		readers,
		data_type.value_or(numerical_type::unknown)
	);
}

void py_read_into(
	host_ndarray destination,
	const image_location &location,
	image_reader_provider &readers
)
{
	const memory_loan loan{const_host_ndarray(destination)};
	auto imported = import_array(destination, loan);

	const nb::gil_scoped_release release;
	read(array_ref(imported), location, readers);
}

std::shared_ptr<completion> py_read_batch_async(
	const image_loader &loader,
	host_ndarray destination,
	const std::vector<image_location> &locations
)
{
	memory_loan loan{const_host_ndarray(destination)};
	auto imported = import_array(destination, loan);

	std::shared_ptr<completion> work;
	{
		const nb::gil_scoped_release release;
		work = read_batch_async(
			loader,
			std::move(imported),
			make_span(locations)
		);
	}

	return std::make_shared<memory_loan_completion>(
		std::move(work),
		std::move(loan)
	);
}

std::shared_ptr<completion> py_read_patches_async(
	const image_loader &loader,
	host_ndarray destination,
	const image_location &location,
	const index_table &centres
)
{
	memory_loan loan{const_host_ndarray(destination)};
	auto imported = import_array(destination, loan);

	std::shared_ptr<completion> work;
	{
		const nb::gil_scoped_release release;
		work = read_patches_async(
			loader,
			std::move(imported),
			location,
			centres
		);
	}

	return std::make_shared<memory_loan_completion>(
		std::move(work),
		std::move(loan)
	);
}

} // anonymous namespace

void bind_image_read(nb::module_ &m)
{
	m.def(
		"read", &py_read_key,
		nb::arg("key"), nb::arg("readers"),
		nb::arg("data_type") = nb::none()
	);
	m.def(
		"read", &py_read_location,
		nb::arg("location"), nb::arg("readers"),
		nb::arg("data_type") = nb::none()
	);
	m.def(
		"read", &py_read_into,
		nb::arg("destination").noconvert(), nb::arg("location"),
		nb::arg("readers")
	);
	m.def(
		"read_batch_async", &py_read_batch_async,
		nb::arg("loader"), nb::arg("destination").noconvert(),
		nb::arg("locations")
	);
	m.def(
		"read_patches_async", &py_read_patches_async,
		nb::arg("loader"), nb::arg("destination").noconvert(),
		nb::arg("location"), nb::arg("centres")
	);
}

} // namespace vitrio
