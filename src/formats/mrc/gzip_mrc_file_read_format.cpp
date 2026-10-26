// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gzip_mrc_file_read_format.hpp"

#include "mrc_constants.hpp"
#include "mrc_extensions.hpp"
#include "mrc_geometry.hpp"
#include "mrc_header.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_metadata.hpp>

#include <compression/gzip_file_stream.hpp>
#include <compression/gzip_identification.hpp>
#include <formats/image_file_format_registration_macros.hpp>
#include <formats/raw/gzip/gzip_image_reader.hpp>

#include <memory>
#include <vector>

namespace vitrio
{
namespace mrc
{

std::string gzip_mrc_file_read_format::get_name() const
{
	return "MRC (gzip)";
}

image_file_format_suitability
gzip_mrc_file_read_format::get_suitability(
	const image_file_probe &probe
) const
{
	const auto header = inflate_leading_bytes(
		probe.get_leading_bytes(),
		header_size
	);

	if (has_map_identifier(make_span(header.data(), header.size())))
	{
		return image_file_format_suitability::normal;
	}

	if (
		is_readable_extension(get_gzip_inner_extension(probe.get_path())) &&
		header.size() >= header_size
	)
	{
		return image_file_format_suitability::fallback;
	}

	return image_file_format_suitability::unsupported;
}

std::shared_ptr<image_reader> gzip_mrc_file_read_format::open(
	const image_file_probe &probe,
	const file_transfer_pool &/*transfers*/
) const
{
	// The leading bytes of the probe may inflate into less than a header,
	// so it is inflated from the file. The values are inflated by each read,
	// on the thread that asks for it.
	std::vector<byte> bytes(header_size);
	gzip_file_stream(probe.get_path()).read(
		make_span(bytes.data(), bytes.size())
	);
	const auto header = parse_header(make_span(bytes.data(), bytes.size()));

	return std::make_shared<gzip_image_reader>(
		probe.get_path(),
		derive_file_layout(
			header,
			get_single_section(get_gzip_inner_extension(probe.get_path()))
		),
		image_metadata()
	);
}

VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(
	gzip_mrc,
	vitrio::mrc::gzip_mrc_file_read_format
);

} // namespace mrc
} // namespace vitrio
