// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "tiff_compression.hpp"
#include "tiff_file_mode.hpp"
#include "tiff_page_layout.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <tiffio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitrio
{
namespace tiff
{

/**
 * @brief One TIFF file, opened through libtiff.
 *
 * A file is a sequence of pages, one of which is selected at a time: what
 * is asked of the file is asked of that page. A file opened for reading
 * starts at its first page, and one being created gains a page with every
 * @ref write_page.
 *
 * What libtiff reports as an error is raised as an exception carrying its
 * message and the path of the file, and what it reports as a warning is
 * logged and otherwise ignored. Neither reaches the handlers libtiff keeps
 * for the whole process.
 *
 * A file is not safe to use from several threads at once.
 */
class tiff_file
{
public:
	/**
	 * @brief Open a file, or create one.
	 *
	 * Creating a file replaces whatever is at that path.
	 *
	 * @param path Path to the file.
	 * @param mode What the file is opened for.
	 * @throws image_file_error If the file can not be reached or created.
	 * @throws image_file_format_error If the file is reached but is not a TIFF
	 * file libtiff can open.
	 */
	tiff_file(const std::string &path, tiff_file_mode mode);

	tiff_file(const tiff_file &other) = delete;
	tiff_file(tiff_file &&other) = delete;
	~tiff_file();

	tiff_file& operator=(const tiff_file &other) = delete;
	tiff_file& operator=(tiff_file &&other) = delete;

	/**
	 * @brief Get how many pages the file holds.
	 *
	 * @return std::size_t The number of pages.
	 */
	std::size_t get_page_count();

	/**
	 * @brief Select the page the file is asked about from here on.
	 *
	 * @param page Index of the page.
	 * @throws image_file_format_error If the file has no such page, or if the
	 * page can not be read.
	 */
	void select_page(std::size_t page);

	/**
	 * @brief Get how the selected page holds its samples.
	 *
	 * @return tiff_page_layout The layout of the page.
	 * @throws image_file_format_error If the page does not state its size, if
	 * it holds anything but one sample per pixel, if its samples have no
	 * counterpart among the data types this format transfers, or if they are
	 * compressed with a scheme this build of libtiff can not decode.
	 */
	tiff_page_layout get_page_layout();

	/**
	 * @brief Decode one block of the selected page.
	 *
	 * The samples come in the byte order of the host, whichever one the file
	 * states them in.
	 *
	 * @param block Index of the block, as @ref tiff_page_layout numbers
	 * them.
	 * @param destination Where the samples are written. Its size must be
	 * that @ref tiff_page_layout::get_block_size gives for @p block.
	 * @throws image_file_format_error If the block can not be decoded, or does
	 * not hold as many samples as @p destination takes.
	 */
	void read_block(std::size_t block, span<byte> destination);

	/**
	 * @brief Get the extents of the selected page, however it is encoded.
	 *
	 * Unlike @ref get_page_layout, nothing but the size of the page is
	 * asked of it, so it answers for a page whose samples libtiff can not
	 * decode.
	 *
	 * @return std::array<std::size_t, 2> The rows and the columns.
	 * @throws image_file_format_error If the page does not state its size.
	 */
	std::array<std::size_t, 2> get_page_extents();

	/**
	 * @brief Get how the samples of the selected page are encoded.
	 *
	 * @return std::uint16_t The Compression tag of the page, which states
	 * none when it is missing.
	 */
	std::uint16_t get_compression();

	/**
	 * @brief Get how many rows every strip of the selected page but the
	 * last one holds.
	 *
	 * @return std::size_t The rows of a strip, which never exceed those of
	 * the page.
	 * @throws image_file_format_error If the page is cut into tiles, if it
	 * does not state its size, or if it states strips of no rows.
	 */
	std::size_t get_rows_per_strip();

	/**
	 * @brief Get how many strips the selected page is cut into.
	 *
	 * @return std::size_t The number of strips.
	 * @throws image_file_format_error If the page is cut into tiles.
	 */
	std::size_t get_strip_count();

	/**
	 * @brief Read one strip of the selected page as it lies in the file,
	 * without decoding it.
	 *
	 * For the pages whose encoding libtiff has no codec for, which are
	 * decoded by whoever reads them.
	 *
	 * @param strip Index of the strip. Must be below @ref get_strip_count.
	 * @param destination Where the bytes are written. It is resized to
	 * hold them, and its capacity is kept for the next strip.
	 * @throws image_file_format_error If the strip can not be read.
	 */
	void read_raw_strip(std::size_t strip, std::vector<byte> &destination);

	/**
	 * @brief Get the value of a tag of the selected page that holds one
	 * unsigned integer, among those libtiff knows nothing of.
	 *
	 * libtiff keeps the tags it does not know without interpreting them.
	 * This reads one of those, as the private tags of a format built on
	 * TIFF are.
	 *
	 * @param tag The code of the tag.
	 * @param value Where its value is written. Left untouched when the
	 * page does not carry the tag.
	 * @return bool true if the page carries the tag.
	 * @throws image_file_format_error If the tag does not hold exactly one
	 * unsigned integer.
	 */
	bool find_unsigned_tag(std::uint32_t tag, std::uint64_t &value);

	/**
	 * @brief Append a page to a file being created.
	 *
	 * @param layout How the page holds its samples. Must be cut into strips.
	 * @param compression How the samples are encoded.
	 * @param samples The samples of the page, row after row, in the byte
	 * order of the host. Their size must be that of a page of @p layout.
	 * They are taken as writable because libtiff is handed them as such:
	 * it reorders the bytes of the samples in place when a file is not in
	 * the byte order of the host. A file created here is, so they are left
	 * as they are.
	 * @throws std::invalid_argument If @p layout is cut into tiles, or if
	 * @p samples do not have the size of the page.
	 * @throws unsupported_operation_error If no sample holds the data type
	 * of @p layout.
	 * @throws image_file_error If the page can not be written.
	 */
	void write_page(
		const tiff_page_layout &layout,
		tiff_compression compression,
		span<byte> samples
	);

private:
	std::string m_path;
	std::string m_last_error;
	TIFF *m_handle;
};

} // namespace tiff
} // namespace vitrio
