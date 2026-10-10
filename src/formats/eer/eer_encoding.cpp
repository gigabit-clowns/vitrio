// SPDX-License-Identifier: LGPL-2.1-or-later

#include "eer_encoding.hpp"

#include <formats/tiff/tiff_file.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <stdexcept>

namespace vitrio
{
namespace eer
{

namespace
{

const std::uint16_t compression_rle8 = 65000;
const std::uint16_t compression_rle7 = 65001;
const std::uint16_t compression_variable = 65002;

const std::uint32_t tag_rle_bits = 65007;
const std::uint32_t tag_horizontal_bits = 65008;
const std::uint32_t tag_vertical_bits = 65009;

const std::size_t max_rle_bits = 16;
const std::size_t max_subpixel_bits = 8;
const std::size_t max_code_bits = 24;

std::size_t read_width(
	tiff::tiff_file &file,
	std::uint32_t tag,
	std::size_t missing
)
{
	std::uint64_t value = missing;
	file.find_unsigned_tag(tag, value);

	// Anything this wide is refused by the encoding it makes.
	return value > max_code_bits
		? max_code_bits + 1
		: static_cast<std::size_t>(value);
}

} // anonymous namespace

eer_encoding::eer_encoding(
	std::size_t rle_bits,
	std::size_t horizontal_bits,
	std::size_t vertical_bits
)
	: m_rle_bits(rle_bits)
	, m_horizontal_bits(horizontal_bits)
	, m_vertical_bits(vertical_bits)
{
	if (rle_bits == 0 || rle_bits > max_rle_bits)
	{
		throw std::invalid_argument(
			"eer_encoding: The run length must be of 1 to 16 bits."
		);
	}
	if (horizontal_bits > max_subpixel_bits ||
		vertical_bits > max_subpixel_bits)
	{
		throw std::invalid_argument(
			"eer_encoding: A subpixel must be of 8 bits at most."
		);
	}
	if (get_code_bits() > max_code_bits)
	{
		throw std::invalid_argument(
			"eer_encoding: A code must be of 24 bits at most."
		);
	}
}

std::size_t eer_encoding::get_rle_bits() const noexcept
{
	return m_rle_bits;
}

std::size_t eer_encoding::get_horizontal_bits() const noexcept
{
	return m_horizontal_bits;
}

std::size_t eer_encoding::get_vertical_bits() const noexcept
{
	return m_vertical_bits;
}

std::size_t eer_encoding::get_code_bits() const noexcept
{
	return m_rle_bits + m_horizontal_bits + m_vertical_bits;
}

bool is_eer_compression(std::uint16_t compression) noexcept
{
	return
		compression == compression_rle8 ||
		compression == compression_rle7 ||
		compression == compression_variable;
}

eer_encoding read_eer_encoding(
	tiff::tiff_file &file,
	const std::string &path
)
{
	switch (file.get_compression())
	{
	case compression_rle8:
		return eer_encoding(8, 2, 2);
	case compression_rle7:
		return eer_encoding(7, 2, 2);
	case compression_variable:
		break;
	default:
		throw image_file_format_error(
			path + ": eer: The page is not compressed with a scheme of EER."
		);
	}

	const auto rle_bits = read_width(file, tag_rle_bits, 7);
	const auto horizontal_bits = read_width(file, tag_horizontal_bits, 2);
	const auto vertical_bits = read_width(file, tag_vertical_bits, 2);
	try
	{
		return eer_encoding(rle_bits, horizontal_bits, vertical_bits);
	}
	catch (const std::invalid_argument &error)
	{
		throw image_file_format_error(
			path + ": eer: The page states an encoding there is not: " +
			error.what()
		);
	}
}

} // namespace eer
} // namespace vitrio
