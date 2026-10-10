// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/library_version.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>

namespace
{

const char *const path = "vitrio_install_consumer.mrc";

vitrio::array make_image(const std::vector<std::size_t> &extents)
{
	auto image = vitrio::make_array(
		vitrio::make_contiguous_array_descriptor(
			vitrio::make_span(extents),
			vitrio::numerical_type::uint8
		)
	);

	const auto count = extents[0] * extents[1];
	for (std::size_t i = 0; i < count; ++i)
	{
		image.get_data()[i] = static_cast<vitrio::byte>(i);
	}

	return image;
}

} // anonymous namespace

// Writes an image and reads it back, which takes the library, its formats
// and a constant of a class that the library has to export.
int main()
{
	std::cout << "vitrio " << vitrio::get_library_version() << "\n";

	const std::vector<std::size_t> extents = {4, 6};
	const auto written = make_image(extents);
	vitrio::write_single(
		written,
		path,
		*vitrio::image_file_write_format_selector::get_shared()
	);

	vitrio::file_image_reader_provider readers(
		vitrio::image_file_read_format_selector::get_shared()
	);
	const vitrio::image_location location(path);
	const std::size_t &no_index = vitrio::image_location::no_stack_index;
	if (location.get_index_in_stack() != no_index)
	{
		std::cerr << "The location of a whole file has a stack index.\n";
		return 1;
	}

	const auto read = vitrio::read(location, readers);
	const auto read_extents = read.get_descriptor().get_extents();
	const auto is_same =
		read_extents.size() == extents.size() &&
		std::equal(extents.begin(), extents.end(), read_extents.begin()) &&
		std::equal(
			written.get_data(),
			written.get_data() + extents[0] * extents[1],
			read.get_data()
		);
	if (!is_same)
	{
		std::cerr << "The image did not read back as it was written.\n";
		return 1;
	}

	return 0;
}
