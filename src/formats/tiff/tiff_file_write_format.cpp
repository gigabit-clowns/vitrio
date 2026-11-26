// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_file_write_format.hpp"

#include "tiff_extensions.hpp"
#include "tiff_image_writer.hpp"
#include "tiff_metadata.hpp"
#include "tiff_sample_type.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/library_version.hpp>

#include <formats/image_file_format_registration_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>

namespace vitrio
{
namespace tiff
{

namespace
{

const std::size_t page_rank = 2;
const std::size_t stack_rank = 3;

// What a file says made it, where nothing else is said to have: the name
// and the version of the software, as the Software tag is meant to hold.
std::string make_signature()
{
	std::ostringstream text;
	text << "vitrio " << get_library_version();
	return text.str();
}

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
	const image_metadata &metadata,
	const file_transfer_pool &/*transfers*/
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

	// A file says this library made it, unless whoever creates it says
	// what did.
	auto tags = make_page_tags(metadata);
	if (tags.get_software().empty())
	{
		tags.set_software(make_signature());
	}

	return std::make_shared<tiff_image_writer>(
		probe.get_path(),
		descriptor,
		std::move(tags)
	);
}

VITRIO_REGISTER_IMAGE_FILE_WRITE_FORMAT(
	tiff,
	vitrio::tiff::tiff_file_write_format
);

} // namespace tiff
} // namespace vitrio
