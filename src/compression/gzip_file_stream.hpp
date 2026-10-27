// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zlib_inflater.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief The inflated bytes of a gzip file, read in order.
 *
 * gzip can only be inflated from its start, so the bytes of the file are
 * reached one after another: those before what is wanted are inflated and
 * skipped. The file is read a stretch at a time, and no more than a stretch
 * of it is held. A file of several members, one after another, reads as the
 * bytes of all of them.
 *
 * What the stream holds past the last byte read is never inflated, so a
 * stream broken there, or a checksum that does not match at its end, goes
 * unnoticed.
 */
class gzip_file_stream
{
public:
	/**
	 * @brief Open a gzip file, to be read @ref get_gzip_read_bytes bytes at
	 * a time.
	 *
	 * @param path Path to the file.
	 * @throws image_file_error If the file can not be opened.
	 */
	explicit gzip_file_stream(const std::string &path);

	/**
	 * @brief Open a gzip file, to be read a given number of bytes at a time.
	 *
	 * @param path Path to the file.
	 * @param read_bytes How many compressed bytes are read at a time. Above
	 * zero.
	 * @throws image_file_error If the file can not be opened.
	 */
	gzip_file_stream(const std::string &path, std::size_t read_bytes);

	gzip_file_stream(const gzip_file_stream &other) = delete;
	gzip_file_stream(gzip_file_stream &&other) = delete;

	~gzip_file_stream() = default;

	gzip_file_stream& operator=(const gzip_file_stream &other) = delete;
	gzip_file_stream& operator=(gzip_file_stream &&other) = delete;

	/**
	 * @brief Pass over inflated bytes without keeping them.
	 *
	 * @param count How many.
	 * @throws image_file_error If the file can not be read.
	 * @throws image_file_format_error If the file is not a gzip stream, or
	 * if its stream ends before as many bytes.
	 */
	void skip(std::uint64_t count);

	/**
	 * @brief Read inflated bytes.
	 *
	 * @param destination Where they go, as many as it holds.
	 * @throws image_file_error If the file can not be read.
	 * @throws image_file_format_error If the file is not a gzip stream, or
	 * if its stream ends before as many bytes.
	 */
	void read(span<byte> destination);

	/**
	 * @brief Get how many inflated bytes have been read or skipped.
	 *
	 * @return std::uint64_t The offset of the next byte, from the first of
	 * the stream.
	 */
	std::uint64_t get_position() const noexcept;

private:
	std::string m_path;
	std::ifstream m_file;
	bool m_file_ended;

	/// What is read of the file at a time.
	std::vector<byte> m_input;

	/// What is read of the file and not yet inflated, within @ref m_input.
	span<const byte> m_pending;

	zlib_inflater m_inflater;
	std::uint64_t m_position;

	/// Where skipped bytes are inflated to, sized the first time.
	std::vector<byte> m_discarded;

	/**
	 * @brief Read the next stretch of the file into @ref m_input.
	 */
	void read_input();
};

} // namespace vitrio
