// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace vitrio
{
namespace tiff
{

class tiff_file;

} // namespace tiff

namespace eer
{

/**
 * @brief How the events of a frame of an EER file are encoded.
 *
 * A frame is a stream of codes read from its least significant bit up. A
 * code begins with a run length: how many pixels, counted row after row, to
 * skip before the next event. A run of the largest value those bits hold
 * skips that many and places no event, and is the run length alone. Any other
 * run places an event on the pixel it reaches, and is followed by where in
 * the pixel the event fell: the horizontal subpixel bits and then the
 * vertical ones.
 *
 * The widths of the three fields are fixed by the compression scheme of the
 * file, or stated by tags of its own for the scheme that lets them vary.
 */
class eer_encoding
{
public:
	/**
	 * @brief Construct an encoding of the given field widths.
	 *
	 * @param rle_bits Width of the run length, in bits.
	 * @param horizontal_bits Width of the horizontal subpixel, in bits.
	 * @param vertical_bits Width of the vertical subpixel, in bits.
	 * @throws std::invalid_argument If @p rle_bits is zero or above 16, if a
	 * subpixel is above 8 bits, or if a code is above 24 bits.
	 */
	eer_encoding(
		std::size_t rle_bits,
		std::size_t horizontal_bits,
		std::size_t vertical_bits
	);

	/**
	 * @brief Get the width of the run length.
	 *
	 * @return std::size_t The width in bits.
	 */
	std::size_t get_rle_bits() const noexcept;

	/**
	 * @brief Get the width of the horizontal subpixel.
	 *
	 * @return std::size_t The width in bits, so a pixel is cut into two to
	 * its power of columns.
	 */
	std::size_t get_horizontal_bits() const noexcept;

	/**
	 * @brief Get the width of the vertical subpixel.
	 *
	 * @return std::size_t The width in bits, so a pixel is cut into two to
	 * its power of rows.
	 */
	std::size_t get_vertical_bits() const noexcept;

	/**
	 * @brief Get the width of a code that places an event.
	 *
	 * @return std::size_t The run length and both subpixels, in bits.
	 */
	std::size_t get_code_bits() const noexcept;

	friend bool operator==(
		const eer_encoding &lhs,
		const eer_encoding &rhs
	) noexcept
	{
		return
			lhs.m_rle_bits == rhs.m_rle_bits &&
			lhs.m_horizontal_bits == rhs.m_horizontal_bits &&
			lhs.m_vertical_bits == rhs.m_vertical_bits;
	}

	friend bool operator!=(
		const eer_encoding &lhs,
		const eer_encoding &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::size_t m_rle_bits;
	std::size_t m_horizontal_bits;
	std::size_t m_vertical_bits;
};

/**
 * @brief Check whether a compression scheme is one of EER.
 *
 * @param compression The Compression tag of a page.
 * @return bool true for 65000, 65001 and 65002.
 */
bool is_eer_compression(std::uint16_t compression) noexcept;

/**
 * @brief Get how the selected page of a file encodes its events.
 *
 * Compression 65000 uses runs of 8 bits and 65001 runs of 7, both with
 * subpixels of 2 bits each way. 65002 states its widths in tags 65007,
 * 65008 and 65009, which stand for 7, 2 and 2 where they are missing.
 *
 * @param file The file, its page selected.
 * @param path Path to the file, which the errors name.
 * @return eer_encoding The encoding.
 * @throws image_file_format_error If the page is not compressed with a
 * scheme of EER, or if its tags state widths no encoding has.
 */
eer_encoding read_eer_encoding(
	tiff::tiff_file &file,
	const std::string &path
);

} // namespace eer
} // namespace vitrio
