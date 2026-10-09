// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mrc_file_write_format.hpp"

#include "mrc_constants.hpp"
#include "mrc_extensions.hpp"
#include "mrc_geometry.hpp"
#include "mrc_header.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>

#include <formats/image_file_format_registration_macros.hpp>
#include <formats/memory_mapping/mapped_image_writer.hpp>

#include <vector>

namespace vitrio
{
namespace mrc
{

std::string mrc_file_write_format::get_name() const
{
	return "MRC";
}

image_file_format_suitability
mrc_file_write_format::get_suitability(const image_file_probe &probe) const
{
	return is_writable_extension(probe.get_extension())
		? image_file_format_suitability::normal
		: image_file_format_suitability::unsupported;
}

std::shared_ptr<image_writer> mrc_file_write_format::open(
	const image_file_probe &probe,
	const image_descriptor &descriptor,
	const image_metadata &/*metadata*/
) const
{
	// Nothing of the metadata reaches the file: image_metadata states
	// nothing yet.
	const auto header = make_header(descriptor);
	const auto layout = derive_file_layout(
		header,
		get_single_section(probe.get_extension())
	);

	// A file that would read back as another shape than it is created with
	// would misplace every region written to it.
	if (layout.get_descriptor() != descriptor)
	{
		throw unsupported_operation_error(
			probe.get_path() + ": mrc_file_write_format: The file would read "
			"back as another shape than it is created with, as a stack of "
			"one image does from a file not named as a stack."
		);
	}

	std::vector<byte> preamble(header_size);
	serialize_header(header, make_span(preamble.data(), preamble.size()));

	return std::make_shared<mapped_image_writer>(
		probe.get_path(),
		layout,
		make_span(preamble.data(), preamble.size())
	);
}

VITRIO_REGISTER_IMAGE_FILE_WRITE_FORMAT(
	mrc,
	vitrio::mrc::mrc_file_write_format
);

} // namespace mrc
} // namespace vitrio
