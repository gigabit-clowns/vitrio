// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <zlib.h>

#include <string>

namespace vitrio
{

/**
 * @brief Inflates a deflate stream, as zlib wraps it, a stretch at a time.
 *
 * The stream is handed over in as many stretches as its holder reads it in,
 * and inflates into as many stretches of output as are given, so that
 * neither side need be held whole. What is inflated comes out in order.
 *
 * A gzip stream may be several members one after another, as `cat`, `pigz`
 * and `bgzip` write them, which inflate into their bytes one after another.
 * An inflater that takes them begins another member wherever input follows
 * the end of one.
 */
class zlib_inflater
{
public:
	/**
	 * @brief Start inflating a stream.
	 *
	 * @param window_bits What zlib's inflateInit2 is given: 15 for a zlib
	 * stream, 31 for a gzip one.
	 * @param multiple_members Whether input that follows the end of the
	 * stream is another member of it, rather than left to the caller.
	 * @param name What the messages it throws start with, such as the name
	 * of a codec or the path of a file.
	 * @throws std::bad_alloc If zlib has no memory for its state.
	 * @throws image_file_format_error If zlib could not start inflating.
	 */
	zlib_inflater(int window_bits, bool multiple_members, std::string name);

	zlib_inflater(const zlib_inflater &other) = delete;
	zlib_inflater(zlib_inflater &&other) = delete;

	~zlib_inflater();

	zlib_inflater& operator=(const zlib_inflater &other) = delete;
	zlib_inflater& operator=(zlib_inflater &&other) = delete;

	/**
	 * @brief Inflate as much of some input into some output as either
	 * allows.
	 *
	 * It stops where the input runs out, where the output is full or where
	 * the stream ends. An inflater that takes several members begins
	 * another one from input given after the end of one; one that does not
	 * leaves that input unread.
	 *
	 * @param input What is left of the stream to hand over. Shrinks from the
	 * front by what is consumed.
	 * @param output Where the bytes go. Shrinks from the front by what is
	 * produced.
	 * @return bool Whether the stream has ended: whether the last member
	 * begun has ended, and nothing has been consumed since.
	 * @throws std::bad_alloc If zlib runs out of memory.
	 * @throws image_file_format_error If the input is not a stream of its
	 * kind, or if a stream that takes several members is followed by bytes
	 * that do not begin another one.
	 */
	bool inflate(span<const byte> &input, span<byte> &output);

	/**
	 * @brief Tell whether the stream has ended.
	 *
	 * @return bool Whether a member has ended, and nothing has been consumed
	 * since. False before anything is consumed.
	 */
	bool has_ended() const noexcept;

	/**
	 * @brief Get the name the messages it throws start with.
	 *
	 * @return const std::string& The name.
	 */
	const std::string& get_name() const noexcept;

private:
	z_stream m_stream;
	std::string m_name;
	bool m_multiple_members;
	bool m_ended;
	/// Whether the member being inflated follows another one.
	bool m_follows_member;
	/// Whether anything was consumed since the last member began.
	bool m_member_started;
};

} // namespace vitrio
