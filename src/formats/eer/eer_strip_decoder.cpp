// SPDX-License-Identifier: LGPL-2.1-or-later

#include "eer_strip_decoder.hpp"

#include <assert.hpp>

namespace vitrio
{
namespace eer
{

namespace
{

std::uint32_t make_mask(std::size_t bits) noexcept
{
	return (std::uint32_t(1) << bits) - 1;
}

// The 32 bits from a bit of the stream up, those past its end taken as
// zero. A code is 24 bits at most and starts within a byte, so it is all
// there.
std::uint32_t read_word(span<const byte> stream, std::uint64_t bit) noexcept
{
	const auto first = static_cast<std::size_t>(bit / 8);
	std::uint32_t word = 0;
	for (std::size_t i = 0; i < 4 && first + i < stream.size(); ++i)
	{
		word |= std::uint32_t(stream[first + i]) << (8 * i);
	}
	return word >> (bit % 8);
}

} // anonymous namespace

eer_strip_decoder::eer_strip_decoder(
	const eer_encoding &encoding,
	std::size_t width
)
	: m_encoding(encoding)
	, m_width(width)
{
	VITRIO_ASSERT(width > 0);
}

bool eer_strip_decoder::decode(
	span<const byte> stream,
	std::size_t first_row,
	std::size_t row_count,
	std::vector<std::uint32_t> &positions
) const
{
	const auto rle_bits = m_encoding.get_rle_bits();
	const auto horizontal_bits = m_encoding.get_horizontal_bits();
	const auto vertical_bits = m_encoding.get_vertical_bits();
	const auto code_bits = m_encoding.get_code_bits();

	const auto rle_mask = make_mask(rle_bits);
	const auto horizontal_mask = make_mask(horizontal_bits);
	const auto vertical_mask = make_mask(vertical_bits);
	const auto horizontal_flip =
		static_cast<std::uint32_t>(horizontal_bits) & horizontal_mask;
	const auto vertical_flip =
		static_cast<std::uint32_t>(vertical_bits) & vertical_mask;

	const auto width = static_cast<std::uint64_t>(m_width);
	const auto pixel_count = width * row_count;
	const auto bit_count = static_cast<std::uint64_t>(stream.size()) * 8;

	std::uint64_t bit = 0;
	std::uint64_t pixel = 0;
	while (pixel < pixel_count && bit_count - bit >= rle_bits)
	{
		const auto word = read_word(stream, bit);
		const auto run = word & rle_mask;

		pixel += run;
		if (pixel == pixel_count)
		{
			break;
		}
		if (pixel > pixel_count)
		{
			return false;
		}
		if (run == rle_mask)
		{
			bit += rle_bits;
			continue;
		}
		if (bit_count - bit < code_bits)
		{
			return false;
		}

		const auto horizontal =
			((word >> rle_bits) & horizontal_mask) ^ horizontal_flip;
		const auto vertical =
			((word >> (rle_bits + horizontal_bits)) & vertical_mask) ^
			vertical_flip;
		const auto row = first_row + pixel / width;
		const auto column = pixel % width;

		positions.push_back(static_cast<std::uint32_t>(
			(row << vertical_bits) | vertical));
		positions.push_back(static_cast<std::uint32_t>(
			(column << horizontal_bits) | horizontal));

		++pixel;
		bit += code_bits;
	}

	return true;
}

} // namespace eer
} // namespace vitrio
