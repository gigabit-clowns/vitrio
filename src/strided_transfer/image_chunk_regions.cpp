// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_chunk_regions.hpp"

#include <array/checked_arithmetic.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace vitrio
{

namespace
{

// One region clipped to one chunk, before the pieces are grouped. Its
// offsets are in the pool of the resolution, from where its values start:
// the offset in the chunk, then the one in the array, then the position of
// the chunk in the grid.
struct piece
{
	std::size_t chunk;
	std::size_t extents;
	std::size_t values;
};

std::vector<std::size_t> get_grid_strides(
	span<const std::size_t> file_extents,
	span<const std::size_t> chunk_extents
)
{
	const auto rank = file_extents.size();
	std::vector<std::size_t> strides(rank, 1);
	std::ptrdiff_t stride = 1;
	for (std::size_t axis = rank; axis-- > 0;)
	{
		strides[axis] = static_cast<std::size_t>(stride);

		const auto chunks =
			file_extents[axis] / chunk_extents[axis] +
			(file_extents[axis] % chunk_extents[axis] != 0 ? 1 : 0);
		if (
			chunks > static_cast<std::size_t>(
				std::numeric_limits<std::ptrdiff_t>::max()) ||
			!checked_multiply(
				stride, static_cast<std::ptrdiff_t>(chunks), stride)
		)
		{
			throw std::overflow_error(
				"image_chunk_regions: The chunks of the file can not be "
				"counted."
			);
		}
	}

	return strides;
}

} // anonymous namespace

image_chunk_regions::image_chunk_regions(
	const image_transfer_plan &regions,
	span<const std::size_t> file_extents,
	span<const std::size_t> chunk_extents
)
	: m_rank(regions.get_shape().get_file_rank())
{
	const auto &shape = regions.get_shape();
	if (file_extents.size() != m_rank || chunk_extents.size() != m_rank)
	{
		throw std::invalid_argument(
			"image_chunk_regions: The extents of the file and of a chunk "
			"must have the rank of the file."
		);
	}
	if (
		std::find(chunk_extents.begin(), chunk_extents.end(), std::size_t(0)) !=
		chunk_extents.end()
	)
	{
		throw std::invalid_argument(
			"image_chunk_regions: The extents of a chunk must not be zero."
		);
	}

	const auto grid_strides = get_grid_strides(file_extents, chunk_extents);
	const auto array_rank = shape.get_array_rank();
	const auto region_rank = shape.get_rank();
	const auto file_leading = m_rank - region_rank;
	const auto array_leading = array_rank - region_rank;
	const auto values_per_piece = m_rank + array_rank + m_rank;

	std::vector<std::size_t> extents(m_rank);
	for (std::size_t axis = 0; axis < m_rank; ++axis)
	{
		extents[axis] = shape.get_extent(m_rank, axis);
	}
	const bool empty =
		std::find(extents.cbegin(), extents.cend(), std::size_t(0)) != extents.cend();

	std::map<std::vector<std::size_t>, std::size_t> interned;
	std::vector<std::vector<std::size_t>> piece_extents;
	std::vector<piece> pieces;
	std::vector<std::size_t> values;

	std::vector<std::size_t> first(m_rank);
	std::vector<std::size_t> last(m_rank);
	std::vector<std::size_t> chunk(m_rank);
	std::vector<std::size_t> clipped(region_rank);
	const auto region_count = empty ? 0 : regions.get_region_count();
	for (std::size_t region = 0; region < region_count; ++region)
	{
		const auto file_offset = regions.get_file_offset(region);
		const auto array_offset = regions.get_array_offset(region);
		for (std::size_t axis = 0; axis < m_rank; ++axis)
		{
			first[axis] = file_offset[axis] / chunk_extents[axis];
			last[axis] =
				(file_offset[axis] + extents[axis] - 1) / chunk_extents[axis];
		}

		chunk = first;
		for (;;)
		{
			const auto start = values.size();
			values.resize(start + values_per_piece);
			auto *const piece_file = values.data() + start;
			auto *const piece_array = piece_file + m_rank;
			auto *const piece_chunk = piece_array + array_rank;

			std::copy(array_offset.begin(), array_offset.end(), piece_array);
			std::size_t linear = 0;
			for (std::size_t axis = 0; axis < m_rank; ++axis)
			{
				// Neither end overflows: the region lies in the file, and
				// the chunk starts before the region ends.
				const auto chunk_start = chunk[axis] * chunk_extents[axis];
				const auto region_end = file_offset[axis] + extents[axis];
				const auto low = std::max(file_offset[axis], chunk_start);
				const auto high = chunk_start + std::min(
					chunk_extents[axis], region_end - chunk_start);

				piece_file[axis] = low - chunk_start;
				piece_chunk[axis] = chunk[axis];
				linear += chunk[axis] * grid_strides[axis];
				if (axis >= file_leading)
				{
					const auto region_axis = axis - file_leading;
					clipped[region_axis] = high - low;
					piece_array[array_leading + region_axis] +=
						low - file_offset[axis];
				}
			}

			const auto found = interned.emplace(
				clipped, piece_extents.size());
			if (found.second)
			{
				piece_extents.push_back(clipped);
			}
			pieces.push_back(piece{linear, found.first->second, start});

			// Step to the next chunk the region reaches, the last axis
			// fastest. Every axis wraps back to its first chunk only once
			// they have all been visited.
			std::size_t axis = m_rank;
			while (axis > 0)
			{
				--axis;
				if (chunk[axis] < last[axis])
				{
					++chunk[axis];
					break;
				}
				chunk[axis] = first[axis];
			}
			if (std::equal(chunk.cbegin(), chunk.cend(), first.cbegin()))
			{
				break;
			}
		}
	}

	std::stable_sort(
		pieces.begin(),
		pieces.end(),
		[] (const piece &lhs, const piece &rhs)
		{
			return std::tie(lhs.chunk, lhs.extents) <
				std::tie(rhs.chunk, rhs.extents);
		}
	);

	for (std::size_t index = 0; index < pieces.size(); ++index)
	{
		const auto &current = pieces[index];
		const auto *const piece_file = values.data() + current.values;
		const auto *const piece_array = piece_file + m_rank;
		const auto *const piece_chunk = piece_array + array_rank;

		const bool new_chunk =
			index == 0 || pieces[index - 1].chunk != current.chunk;
		if (new_chunk)
		{
			m_chunks.insert(m_chunks.end(), piece_chunk, piece_chunk + m_rank);
			m_first_plans.push_back(m_plans.size());
		}
		if (new_chunk || pieces[index - 1].extents != current.extents)
		{
			m_plans.emplace_back(
				image_transfer_shape(
					piece_extents[current.extents],
					m_rank,
					array_rank
				)
			);
		}

		m_plans.back().add(
			make_span(piece_file, m_rank),
			make_span(piece_array, array_rank)
		);
	}
	m_first_plans.push_back(m_plans.size());
}

std::size_t image_chunk_regions::get_chunk_count() const noexcept
{
	return m_first_plans.size() - 1;
}

span<const std::size_t>
image_chunk_regions::get_chunk(std::size_t position) const noexcept
{
	return make_span(m_chunks.data() + position * m_rank, m_rank);
}

std::size_t
image_chunk_regions::get_plan_count(std::size_t position) const noexcept
{
	return m_first_plans[position + 1] - m_first_plans[position];
}

const image_transfer_plan& image_chunk_regions::get_regions(
	std::size_t position,
	std::size_t plan
) const noexcept
{
	return m_plans[m_first_plans[position] + plan];
}

} // namespace vitrio
