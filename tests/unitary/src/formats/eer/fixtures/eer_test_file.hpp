// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{
namespace test
{

/**
 * @brief One event of a frame, as a test states it: the pixel it fell on
 * and where in the pixel, the subpixels as they are decoded.
 */
struct eer_test_event
{
	std::size_t row;
	std::size_t column;
	std::uint32_t horizontal;
	std::uint32_t vertical;
};

/**
 * @brief Appends codes to a stream, from its least significant bit up.
 */
class eer_bit_writer
{
public:
	void write(std::uint32_t code, std::size_t bits)
	{
		for (std::size_t i = 0; i < bits; ++i, ++m_bit)
		{
			if (m_bit % 8 == 0)
			{
				m_bytes.push_back(0);
			}
			if ((code >> i) & 1u)
			{
				m_bytes.back() = static_cast<byte>(
					m_bytes.back() | (1u << (m_bit % 8)));
			}
		}
	}

	std::vector<byte> finish() const
	{
		auto bytes = m_bytes;
		bytes.resize(bytes.size() + bytes.size() % 2 + 2, 0);
		return bytes;
	}

private:
	std::vector<byte> m_bytes;
	std::size_t m_bit = 0;
};

/**
 * @brief Encode the events of one strip as an EER file encodes them.
 *
 * Runs too long for a code are cut into codes of the largest run, which
 * place no event, and the stream ends with a run to the last pixel of the
 * strip. A subpixel is stored with the bit its width counts flipped.
 *
 * @param pixels The events, as their pixel counted from the first of the
 * strip, ascending, and their subpixels.
 * @param pixel_count Number of pixels of the strip.
 * @param rle_bits Width of a run.
 * @param horizontal_bits Width of the horizontal subpixel.
 * @param vertical_bits Width of the vertical subpixel.
 * @return std::vector<byte> The stream, padded to an even number of bytes
 * and two more.
 */
inline std::vector<byte> encode_eer_strip(
	const std::vector<std::pair<std::size_t, eer_test_event>> &pixels,
	std::size_t pixel_count,
	std::size_t rle_bits,
	std::size_t horizontal_bits,
	std::size_t vertical_bits
)
{
	const auto most = (std::size_t(1) << rle_bits) - 1;
	const auto code_bits = rle_bits + horizontal_bits + vertical_bits;
	eer_bit_writer stream;

	std::size_t position = 0;
	for (const auto &pixel : pixels)
	{
		auto gap = pixel.first - position;
		for (; gap >= most; gap -= most)
		{
			stream.write(static_cast<std::uint32_t>(most), rle_bits);
		}

		const auto horizontal = pixel.second.horizontal ^
			static_cast<std::uint32_t>(horizontal_bits);
		const auto vertical = pixel.second.vertical ^
			static_cast<std::uint32_t>(vertical_bits);
		stream.write(
			static_cast<std::uint32_t>(gap) |
				(horizontal << rle_bits) |
				(vertical << (rle_bits + horizontal_bits)),
			code_bits
		);
		position = pixel.first + 1;
	}

	auto gap = pixel_count - position;
	for (; gap >= most; gap -= most)
	{
		stream.write(static_cast<std::uint32_t>(most), rle_bits);
	}
	if (gap > 0)
	{
		stream.write(static_cast<std::uint32_t>(gap), code_bits);
	}

	return stream.finish();
}

/**
 * @brief What a test writes an EER file with.
 */
struct eer_test_movie
{
	std::uint16_t compression = 65001;
	std::size_t width = 8;
	std::size_t height = 6;
	std::size_t rows_per_strip = 3;
	std::size_t rle_bits = 7;
	std::size_t horizontal_bits = 2;
	std::size_t vertical_bits = 2;
	/// Whether tags 65007 to 65009 state the widths, as 65002 does.
	bool states_widths = false;
	/// The events of each frame, in the order of their pixels.
	std::vector<std::vector<eer_test_event>> frames;
};

/**
 * @brief Write a little-endian BigTIFF file of pages already encoded, by
 * hand rather than through libtiff, which writes no page it has no codec
 * for.
 *
 * @param path Path to the file.
 * @param pages The tags of each page, a code mapped to a type and its
 * values, and its strips, whose offsets and byte counts are filled in here.
 */
inline void write_big_tiff(
	const std::string &path,
	const std::vector<std::pair<
		std::map<std::uint16_t, std::pair<std::uint16_t,
			std::vector<std::uint64_t>>>,
		std::vector<std::vector<byte>>
	>> &pages
)
{
	std::vector<byte> data = {'I', 'I', 43, 0, 8, 0, 0, 0};
	data.resize(16, 0);

	const auto put = [&data] (std::size_t at, std::uint64_t value, int size)
	{
		for (int i = 0; i < size; ++i)
		{
			data[at + i] = static_cast<byte>(value >> (8 * i));
		}
	};
	const auto align = [&data] ()
	{
		while (data.size() % 8 != 0)
		{
			data.push_back(0);
		}
	};
	const auto type_size = [] (std::uint16_t type)
	{
		return type == 3 ? 2 : type == 4 ? 4 : type == 16 ? 8 : 1;
	};

	std::size_t previous = 8;
	for (const auto &page : pages)
	{
		auto tags = page.first;
		std::vector<std::uint64_t> offsets;
		std::vector<std::uint64_t> counts;
		for (const auto &strip : page.second)
		{
			align();
			offsets.push_back(data.size());
			counts.push_back(strip.size());
			data.insert(data.end(), strip.begin(), strip.end());
		}
		tags[273] = {16, offsets};
		tags[279] = {16, counts};

		align();
		const auto directory = data.size();
		std::size_t values = directory + 8 + 20 * tags.size() + 8;
		std::vector<byte> spilled;

		data.resize(data.size() + 8 + 20 * tags.size() + 8, 0);
		put(directory, tags.size(), 8);
		std::size_t entry = directory + 8;
		for (const auto &tag : tags)
		{
			const auto type = tag.second.first;
			const auto &value = tag.second.second;
			const auto size = type_size(type);

			put(entry, tag.first, 2);
			put(entry + 2, type, 2);
			put(entry + 4, value.size(), 8);
			if (value.size() * size <= 8)
			{
				for (std::size_t i = 0; i < value.size(); ++i)
				{
					put(entry + 12 + i * size, value[i], size);
				}
			}
			else
			{
				while (spilled.size() % 8 != 0)
				{
					spilled.push_back(0);
				}
				put(entry + 12, values + spilled.size(), 8);
				for (const auto item : value)
				{
					for (int i = 0; i < size; ++i)
					{
						spilled.push_back(static_cast<byte>(item >> (8 * i)));
					}
				}
			}
			entry += 20;
		}

		put(previous, directory, 8);
		previous = entry;
		data.insert(data.end(), spilled.begin(), spilled.end());
	}

	std::ofstream file(path.c_str(), std::ios::binary);
	file.write(reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size()));
	if (!file)
	{
		throw std::runtime_error("The test file could not be written.");
	}
}

/**
 * @brief One page of an EER file, its strips already encoded.
 */
struct eer_test_page
{
	std::uint16_t compression = 65001;
	std::size_t width = 8;
	std::size_t height = 6;
	std::size_t rows_per_strip = 3;
	/// The widths tags 65007 to 65009 state, or none when empty.
	std::vector<std::size_t> stated_widths;
	std::vector<std::vector<byte>> strips;
};

/**
 * @brief Write an EER file of pages already encoded.
 *
 * @param path Path to the file.
 * @param pages The pages, one per frame.
 */
inline void write_eer_pages(
	const std::string &path,
	const std::vector<eer_test_page> &pages
)
{
	using tags = std::map<
		std::uint16_t, std::pair<std::uint16_t, std::vector<std::uint64_t>>
	>;
	std::vector<std::pair<tags, std::vector<std::vector<byte>>>> written;

	for (const auto &page : pages)
	{
		tags page_tags = {
			{256, {4, {page.width}}},
			{257, {4, {page.height}}},
			{258, {3, {8}}},
			{259, {3, {page.compression}}},
			{262, {3, {1}}},
			{277, {3, {1}}},
			{278, {4, {page.rows_per_strip}}},
		};
		for (std::size_t i = 0; i < page.stated_widths.size(); ++i)
		{
			page_tags[static_cast<std::uint16_t>(65007 + i)] =
				{3, {page.stated_widths[i]}};
		}
		written.emplace_back(page_tags, page.strips);
	}

	write_big_tiff(path, written);
}

/**
 * @brief Encode the frames of a movie into the pages of an EER file.
 *
 * @param movie What the frames hold and how they are encoded.
 * @return std::vector<eer_test_page> A page per frame.
 */
inline std::vector<eer_test_page> make_eer_pages(const eer_test_movie &movie)
{
	std::vector<eer_test_page> pages;
	for (const auto &events : movie.frames)
	{
		eer_test_page page;
		page.compression = movie.compression;
		page.width = movie.width;
		page.height = movie.height;
		page.rows_per_strip = movie.rows_per_strip;
		if (movie.states_widths)
		{
			page.stated_widths = {
				movie.rle_bits,
				movie.horizontal_bits,
				movie.vertical_bits
			};
		}

		for (std::size_t first = 0; first < movie.height;
			first += movie.rows_per_strip)
		{
			const auto rows = std::min(
				movie.rows_per_strip, movie.height - first);
			std::vector<std::pair<std::size_t, eer_test_event>> pixels;
			for (const auto &event : events)
			{
				if (event.row >= first && event.row < first + rows)
				{
					pixels.emplace_back(
						(event.row - first) * movie.width + event.column,
						event
					);
				}
			}
			page.strips.push_back(encode_eer_strip(
				pixels,
				rows * movie.width,
				movie.rle_bits,
				movie.horizontal_bits,
				movie.vertical_bits
			));
		}

		pages.push_back(page);
	}
	return pages;
}

/**
 * @brief Write an EER file holding given events.
 *
 * @param path Path to the file.
 * @param movie What it holds and how it is encoded.
 */
inline void write_eer_file(const std::string &path, const eer_test_movie &movie)
{
	write_eer_pages(path, make_eer_pages(movie));
}

} // namespace test
} // namespace vitrio
