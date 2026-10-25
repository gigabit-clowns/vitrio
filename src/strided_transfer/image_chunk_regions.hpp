// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief The regions of a plan, resolved into those of each chunk they
 * reach.
 *
 * A file that holds its values in chunks, bricks of one shape that tile it
 * along every axis as a ZARR array does, has the regions stated against the
 * whole of it moved a chunk at a time. Each chunk a plan reaches gets the
 * pieces of the regions that lie on it, stated against that chunk alone: as
 * offsets into it, of the rank of the file, and with the array offsets moved
 * to where each piece lands. To its pieces, a chunk is a file of the extents
 * of one chunk, at the edges of the file too, where it reaches past them.
 *
 * The pieces of a region share no extents with those of another unless the
 * two are clipped alike, so the pieces of a chunk are held as several plans,
 * one for each of the extents among them.
 *
 * The chunks are held in ascending order of their position in the grid,
 * with the last axis the fastest.
 */
class image_chunk_regions
{
public:
	/**
	 * @brief Resolve the regions of a plan into those of each chunk.
	 *
	 * Takes a time linear in the pieces the regions are cut into, and
	 * whatever the number of chunks of the file.
	 *
	 * @param regions The regions. They must be contained in the file.
	 * @param file_extents Extents of the file, of the file rank of
	 * @p regions.
	 * @param chunk_extents Extents of one chunk, of the same rank.
	 * @throws std::invalid_argument If @p file_extents or @p chunk_extents
	 * does not have the file rank of @p regions, or if an extent of a chunk
	 * is zero.
	 * @throws std::overflow_error If the chunks of the file can not be
	 * counted.
	 */
	image_chunk_regions(
		const image_transfer_plan &regions,
		span<const std::size_t> file_extents,
		span<const std::size_t> chunk_extents
	);

	image_chunk_regions(const image_chunk_regions &other) = default;
	image_chunk_regions(image_chunk_regions &&other) noexcept = default;
	~image_chunk_regions() = default;

	image_chunk_regions& operator=(const image_chunk_regions &other) = default;
	image_chunk_regions&
	operator=(image_chunk_regions &&other) noexcept = default;

	/**
	 * @brief Get how many chunks the regions reach.
	 *
	 * @return std::size_t The number of chunks.
	 */
	std::size_t get_chunk_count() const noexcept;

	/**
	 * @brief Get the position in the grid of one of the chunks reached.
	 *
	 * @param position Position among the chunks reached. Must be below
	 * @ref get_chunk_count.
	 * @return span<const std::size_t> The index of the chunk along each axis
	 * of the file, which is its offset divided by the extents of a chunk. It
	 * refers to storage owned by this object.
	 */
	span<const std::size_t> get_chunk(std::size_t position) const noexcept;

	/**
	 * @brief Get how many plans hold the pieces that lie on one chunk.
	 *
	 * @param position Position among the chunks reached. Must be below
	 * @ref get_chunk_count.
	 * @return std::size_t The number of plans, one for each of the extents
	 * among the pieces. Never zero.
	 */
	std::size_t get_plan_count(std::size_t position) const noexcept;

	/**
	 * @brief Get one of the plans of the pieces that lie on one chunk.
	 *
	 * @param position Position among the chunks reached. Must be below
	 * @ref get_chunk_count.
	 * @param plan Which of its plans. Must be below @ref get_plan_count.
	 * @return const image_transfer_plan& The pieces, stated against a file
	 * of the extents of a chunk and the array of the plan they were resolved
	 * from. It refers to storage owned by this object.
	 */
	const image_transfer_plan& get_regions(
		std::size_t position,
		std::size_t plan
	) const noexcept;

private:
	std::size_t m_rank;
	std::vector<std::size_t> m_chunks;
	std::vector<std::size_t> m_first_plans;
	std::vector<image_transfer_plan> m_plans;
};

} // namespace vitrio
