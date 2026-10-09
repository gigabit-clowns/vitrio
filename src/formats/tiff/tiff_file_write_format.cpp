// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_file_write_format.hpp"

#include "tiff_extensions.hpp"
#include "tiff_sample_type.hpp"
#include "tiff_writer.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>

#include <formats/image_file_format_registration_macros.hpp>

#include <algorithm>
#include <cstddef>

namespace vitrio
{
namespace tiff
{

namespace
{

const std::size_t page_rank = 2;
const std::size_t stack_rank = 3;

} // anonymous namespace

std::string tiff_file_write_format::get_name() const
{
	return "TIFF";
}

image_file_format_suitability
tiff_file_write_format::get_suitability(const image_file_probe &probe) const
{
	return is_writable_extension(probe.get_extension())
		? image_file_format_suitability::normal
		: image_file_format_suitability::unsupported;
}

std::shared_ptr<image_writer> tiff_file_write_format::open(
	const image_file_probe &probe,
	const image_descriptor &descriptor,
	const image_metadata &/*metadata*/
) const
{
	const auto extents = descriptor.get_extents();
	const auto rank = extents.size();
	if ((rank != page_rank && rank != stack_rank) ||
		descriptor.get_core_rank() != page_rank)
	{
		throw unsupported_operation_error(
			probe.get_path() + ": tiff_file_write_format: A TIFF file holds an "
			"image or a stack of images."
		);
	}

	// A file of one page reads back as an image, so a stack of one would
	// read back as another shape than it is created with.
	if (rank == stack_rank && extents[0] == 1)
	{
		throw unsupported_operation_error(
			probe.get_path() + ": tiff_file_write_format: A stack of one image "
			"would read back as a single image."
		);
	}

	if (std::find(extents.begin(), extents.end(), 0) != extents.end())
	{
		throw unsupported_operation_error(
			probe.get_path() + ": tiff_file_write_format: A TIFF file has no "
			"extent of zero."
		);
	}

	if (!is_supported(descriptor.get_data_type()))
	{
		throw unsupported_operation_error(
			probe.get_path() + ": tiff_file_write_format: The TIFF format has "
			"no sample this format transfers for this data type."
		);
	}

	return std::make_shared<tiff_writer>(probe.get_path(), descriptor);
}

VITRIO_REGISTER_IMAGE_FILE_WRITE_FORMAT(
	tiff,
	vitrio::tiff::tiff_file_write_format
);

} // namespace tiff
} // namespace vitrio
