// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief A sequence of indices of one and the same rank.
 *
 * Each index is a tuple of one coordinate per axis. The indices are held in
 * one flat vector of @ref get_index_count by @ref get_rank values rather
 * than one vector each, so a table of any length costs a bounded number of
 * allocations, and @ref clear keeps the capacity: a table refilled after
 * clearing allocates nothing until it outgrows what it held before.
 *
 * The rank is stated when a table is constructed and never changes, so every
 * index it holds carries it. A table of rank zero holds indices of no
 * coordinates, each addressing the single position a space of rank zero
 * has.
 */
class index_table
{
public:
	/**
	 * @brief Construct an empty table of rank zero.
	 */
	VITRIO_API
	index_table() noexcept;

	/**
	 * @brief Construct an empty table of a given rank.
	 *
	 * @param rank Number of coordinates each index carries.
	 */
	VITRIO_API
	explicit index_table(std::size_t rank);

	VITRIO_API
	index_table(const index_table &other);
	VITRIO_API
	index_table(index_table &&other) noexcept;
	VITRIO_API
	~index_table();

	VITRIO_API
	index_table& operator=(const index_table &other);
	VITRIO_API
	index_table& operator=(index_table &&other) noexcept;

	/**
	 * @brief Append one index.
	 *
	 * @param index The coordinates. Their number must equal @ref get_rank.
	 * @throws std::invalid_argument If @p index does not have the rank of
	 * this table.
	 */
	VITRIO_API
	void add(span<const std::size_t> index);

	/**
	 * @brief Drop the indices held, keeping the rank and the capacity.
	 */
	VITRIO_API
	void clear() noexcept;

	/**
	 * @brief Make room for a number of indices without allocating later.
	 *
	 * @param count Number of indices to make room for.
	 */
	VITRIO_API
	void reserve(std::size_t count);

	/**
	 * @brief Get the number of coordinates each index carries.
	 *
	 * @return std::size_t The rank.
	 */
	VITRIO_API
	std::size_t get_rank() const noexcept;

	/**
	 * @brief Get how many indices are held.
	 *
	 * @return std::size_t The number of indices.
	 */
	VITRIO_API
	std::size_t get_index_count() const noexcept;

	/**
	 * @brief Get one index.
	 *
	 * @param position Position of the index. Must be below
	 * @ref get_index_count.
	 * @return span<const std::size_t> The coordinates, of rank
	 * @ref get_rank. It refers to storage owned by this table, which adding
	 * to, assigning to or destroying it invalidates.
	 */
	VITRIO_API
	span<const std::size_t> get(std::size_t position) const noexcept;

private:
	std::vector<std::size_t> m_values;
	std::size_t m_rank;
	std::size_t m_size;
};

} // namespace vitrio
