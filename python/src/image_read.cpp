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
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/image_loader.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch.hpp>
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

// Each file is opened and read on the executor, but a synchronous one runs
// the work before the call returns.
std::shared_ptr<completion> py_prefetch_scratch_async(
	image_scratch &scratch,
	std::shared_ptr<image_reader_provider> files,
	vitrio::executor &executor,
	const image_location_grouping &locations
)
{
	const nb::gil_scoped_release release;
	return prefetch_scratch_async(
		scratch,
		std::move(files),
		executor,
		locations
	);
}

} // anonymous namespace

void bind_image_read(nb::module_ &m)
{
	m.def(
		"read", &py_read_key,
		nb::arg("key"), nb::arg("readers"),
		nb::arg("data_type") = nb::none(),
		"Read a whole file into an array of its own.\n"
		"\n"
		"Prefer `vitrio.read`, which defaults the readers.\n"
		"\n"
		"Args:\n"
		"    key: The file, as its key in `readers`.\n"
		"    readers: Where the file becomes a reader.\n"
		"    data_type: The type to read the values as. Defaults to the one "
		"the file holds.\n"
		"\n"
		"Returns:\n"
		"    Array: What the file holds."
	);
	m.def(
		"read", &py_read_location,
		nb::arg("location"), nb::arg("readers"),
		nb::arg("data_type") = nb::none(),
		"Read one image or volume of a file into an array of its own.\n"
		"\n"
		"Args:\n"
		"    location: An `ImageLocation`.\n"
		"    readers: Where the file becomes a reader.\n"
		"    data_type: The type to read the values as. Defaults to the one "
		"the file holds.\n"
		"\n"
		"Returns:\n"
		"    Array: The image or volume."
	);
	m.def(
		"read", &py_read_into,
		nb::arg("destination").noconvert(), nb::arg("location"),
		nb::arg("readers"),
		"Read an image, a volume or a file into an array of yours.\n"
		"\n"
		"Args:\n"
		"    destination: Where the values land, as any host array. Its shape "
		"has to be that of what `location` names, and its data type is the "
		"one the values are read as.\n"
		"    location: An `ImageLocation`.\n"
		"    readers: Where the file becomes a reader."
	);
	m.def(
		"read_batch_async", &py_read_batch_async,
		nb::arg("loader"), nb::arg("destination").noconvert(),
		nb::arg("locations"),
		"Read one image or volume per location into a batch.\n"
		"\n"
		"Returns before the reads are done. Slot `i` of `destination` "
		"receives the image `locations[i]` names, or the whole file when it "
		"names no index in a stack. A batch may not mix the two. The images "
		"may come from any number of files, and the reads run on the loader's "
		"executor.\n"
		"\n"
		"Args:\n"
		"    loader: The `ImageLoader` the reads are dispatched to.\n"
		"    destination: Where the images land, as any host array, written "
		"where it is. Its leading extent is the batch size and the rest are "
		"the shape of one image. Its data type is the one the values are read "
		"as.\n"
		"    locations: A list of `ImageLocation`, one for each slot, in "
		"order.\n"
		"\n"
		"Returns:\n"
		"    Completion: Ready once every image is in or has failed. It keeps "
		"`destination` alive, and destroying it waits for the reads. Its "
		"`get` raises the first failure, such as an index its stack does not "
		"hold.\n"
		"\n"
		"Raises:\n"
		"    ValueError: If the leading extent of `destination` is not the "
		"number of locations, or if the locations mix those that name an "
		"index with those that name none."
	);
	m.def(
		"read_patches_async", &py_read_patches_async,
		nb::arg("loader"), nb::arg("destination").noconvert(),
		nb::arg("location"), nb::arg("centres"),
		"Crop patches of equal size out of one image.\n"
		"\n"
		"Prefer `vitrio.read_patches_async`, which takes the centres as an "
		"array or a list.\n"
		"\n"
		"Args:\n"
		"    loader: The `ImageLoader` the reads are dispatched to.\n"
		"    destination: Where the patches land. Its leading extent is the "
		"number of patches and the rest are the shape of one.\n"
		"    location: The `ImageLocation` of the image they are cropped "
		"from.\n"
		"    centres: An `IndexTable` with a centre for each patch.\n"
		"\n"
		"Returns:\n"
		"    Completion: Ready once every patch has been read or has failed."
	);
	m.def(
		"prefetch_scratch_async", &py_prefetch_scratch_async,
		nb::arg("scratch"), nb::arg("files"), nb::arg("executor"),
		nb::arg("locations"),
		"Take images into a scratch ahead of the reads that need them.\n"
		"\n"
		"Prefer `vitrio.prefetch_scratch_async`, which defaults the readers.\n"
		"\n"
		"Args:\n"
		"    scratch: The `ImageScratch` to fill.\n"
		"    files: The `ImageReaderProvider` that reads the files "
		"themselves, not through the scratch.\n"
		"    executor: Where the files are read, a task for each.\n"
		"    locations: An `ImageLocationGrouping` of the images to take in.\n"
		"\n"
		"Returns:\n"
		"    Completion: Ready once every file has been taken in or has "
		"failed."
	);
}

} // namespace vitrio
