// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>

#include <cstddef>
#include <mutex>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The buffers the chunks of ZARR arrays are read into and decoded
 * into, kept from one read to the next.
 *
 * A read takes a buffer of the bytes of each chunk it reads and of each it
 * decodes, and gives them back once the chunk is in the array. A buffer
 * kept holds its memory: the next chunk that fits it lands in pages the
 * system has already given, sized without being filled, where a fresh
 * buffer costs a page fault for each page and a fill of zeros, on every
 * chunk, and more under many threads at once, which is most of what a
 * cached chunk costs. Most of the bytes kept at once are a budget, past
 * which a buffer given back is released.
 *
 * The buffer taken is the last one given back that fits, which is the
 * warmest. Taking and giving are safe from any thread.
 */
class zarr_chunk_buffers
{
public:
	/**
	 * @brief Make a store of buffers that holds none yet.
	 *
	 * @param byte_budget Most bytes of buffers kept at once, counting the
	 * memory each holds.
	 */
	explicit zarr_chunk_buffers(std::size_t byte_budget) noexcept;

	/**
	 * @brief Take a buffer of some bytes.
	 *
	 * @param size The bytes.
	 * @return The buffer given back last that holds at least @p size bytes
	 * of memory, sized to @p size with whatever it held, or a new one of
	 * @p size bytes when none kept fits.
	 */
	std::vector<byte> take(std::size_t size);

	/**
	 * @brief Give a buffer back, to be taken again.
	 *
	 * It is kept if the memory it holds fits what is left of the budget,
	 * and released otherwise.
	 *
	 * @param buffer The buffer.
	 */
	void give(std::vector<byte> buffer) noexcept;

	/**
	 * @brief The bytes of the buffers kept now.
	 */
	std::size_t get_kept_bytes() const;

private:
	mutable std::mutex m_mutex;
	std::vector<std::vector<byte>> m_buffers;
	std::size_t m_bytes;
	std::size_t m_byte_budget;
};

} // namespace zarr
} // namespace vitrio
