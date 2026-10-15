// SPDX-License-Identifier: LGPL-2.1-or-later

// The smallest program that uses vitrio: it writes an image, reads it back
// converted to another data type, and compares the two. It makes its own
// input and removes what it writes.

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/image_write.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>

int main()
{
	const char *const path = "vitrio_example_read_and_convert.mrc";

	// An image of 4 rows and 6 columns, of 16-bit integers.
	const std::vector<std::size_t> extents = {4, 6};
	auto image = vitrio::make_array(
		vitrio::make_contiguous_array_descriptor(
			vitrio::make_span(extents),
			vitrio::numerical_type::int16
		)
	);
	auto *values = reinterpret_cast<std::int16_t *>(image.get_data());
	for (std::size_t i = 0; i < extents[0] * extents[1]; ++i)
	{
		values[i] = static_cast<std::int16_t>(i);
	}

	// The format is chosen from what the selector holds and the path.
	vitrio::write_single(
		image,
		path,
		*vitrio::image_file_write_format_selector::get_shared()
	);

	// Read as 32-bit floats, whatever the file holds.
	vitrio::file_image_reader_provider readers(
		vitrio::image_file_read_format_selector::get_shared()
	);
	const vitrio::array converted =
		vitrio::read(path, readers, vitrio::numerical_type::float32);
	std::remove(path);

	const auto *read =
		reinterpret_cast<const float *>(converted.get_data());
	for (std::size_t i = 0; i < extents[0] * extents[1]; ++i)
	{
		if (read[i] != static_cast<float>(values[i]))
		{
			std::cerr << "Pixel " << i << " did not survive.\n";
			return 1;
		}
	}

	std::cout << "Read " << extents[0] << " by " << extents[1]
	          << " pixels as float32.\n";
	return 0;
}
