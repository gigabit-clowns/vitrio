// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "eer_encoding.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vitrio
{
namespace eer
{

/**
 * @brief Decodes the strips of the frames of an EER file into the positions
 * of their events.
 *
 * Each strip of a frame is a stream of its own, which places its events on
 * the rows of that strip, counting pixels from its first one. A stream ends
 * where its runs reach the last pixel of the strip, or where it runs out of
 * bits.
 *
 * A position is a row and then a column of the grid finer than the pixels
 * by the subpixel bits: the pixel shifted up by those bits, joined with the
 * subpixel. A subpixel is stored with the bit its width counts flipped, so
 * that two bits store 2 for the subpixel 0, as the reference reader of the
 * format decodes it.
 */
class eer_strip_decoder
{
public:
	/**
	 * @brief Construct a decoder of the strips of frames of a given width.
	 *
	 * @param encoding How the events are encoded.
	 * @param width Number of columns of pixels of a frame. Must not be zero.
	 */
	eer_strip_decoder(const eer_encoding &encoding, std::size_t width);

	/**
	 * @brief Decode one strip, appending the positions of its events.
	 *
	 * @param stream The bytes of the strip as they lie in the file.
	 * @param first_row Index of the first row of pixels of the strip.
	 * @param row_count Number of rows of pixels of the strip.
	 * @param positions Where the positions are appended, a row and a column
	 * per event. What was appended stays there when the strip turns out to
	 * be malformed.
	 * @return bool false if the strip places an event past its last pixel,
	 * or ends within a code that places one.
	 */
	bool decode(
		span<const byte> stream,
		std::size_t first_row,
		std::size_t row_count,
		std::vector<std::uint32_t> &positions
	) const;

private:
	eer_encoding m_encoding;
	std::size_t m_width;
};

} // namespace eer
} // namespace vitrio
