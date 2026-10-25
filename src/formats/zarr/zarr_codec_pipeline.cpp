// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_codec_pipeline.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vitrio
{
namespace zarr
{

zarr_codec_pipeline::zarr_codec_pipeline() noexcept = default;

zarr_codec_pipeline::zarr_codec_pipeline(
	std::vector<std::shared_ptr<const zarr_bytes_codec>> codecs
)
	: m_codecs(std::move(codecs))
{
	if (
		std::find(m_codecs.cbegin(), m_codecs.cend(), nullptr) !=
		m_codecs.cend()
	)
	{
		throw std::invalid_argument(
			"zarr_codec_pipeline: A codec must not be null."
		);
	}
}

zarr_codec_pipeline::zarr_codec_pipeline(
	const zarr_codec_pipeline &other) = default;
zarr_codec_pipeline::zarr_codec_pipeline(
	zarr_codec_pipeline &&other) noexcept = default;
zarr_codec_pipeline::~zarr_codec_pipeline() = default;

zarr_codec_pipeline&
zarr_codec_pipeline::operator=(const zarr_codec_pipeline &other) = default;
zarr_codec_pipeline&
zarr_codec_pipeline::operator=(zarr_codec_pipeline &&other) noexcept = default;

bool zarr_codec_pipeline::is_empty() const noexcept
{
	return m_codecs.empty();
}

std::size_t zarr_codec_pipeline::get_codec_count() const noexcept
{
	return m_codecs.size();
}

void zarr_codec_pipeline::decode(
	span<const byte> encoded,
	std::size_t size,
	std::vector<byte> &decoded
) const
{
	if (m_codecs.empty())
	{
		decoded.assign(encoded.begin(), encoded.end());
	}

	// Every step but the last writes into one of two buffers in turn, and
	// the last into what the caller gave.
	std::vector<byte> buffers[2];
	auto input = encoded;
	for (std::size_t step = 0; step < m_codecs.size(); ++step)
	{
		const auto &codec = *m_codecs[m_codecs.size() - 1 - step];
		const bool last = step + 1 == m_codecs.size();
		auto &output = last ? decoded : buffers[step % 2];

		codec.decode(input, last ? size : std::max(size, input.size()), output);
		input = make_span(
			static_cast<const byte*>(output.data()), output.size());
	}

	if (decoded.size() != size)
	{
		throw image_file_format_error(
			"zarr_codec_pipeline: The chunk does not decode into as many "
			"bytes as it holds values."
		);
	}
}

} // namespace zarr
} // namespace vitrio
