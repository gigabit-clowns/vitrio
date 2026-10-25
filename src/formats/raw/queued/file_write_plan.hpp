// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_transfer_completion.hpp>

#include <vitrio/const_memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief Writes of stretches of a file, each gathered from buffers, grouped.
 *
 * A group is what is written together, once it is ready, such as the
 * stretches one staged stretch of a file is written as. The writes are held
 * in the order they were added, each in the group that was opened last.
 */
class file_write_plan
{
public:
	/**
	 * @brief Construct a plan of no group.
	 */
	file_write_plan() noexcept;

	/**
	 * @brief Open a group, which the writes added from now on fall in.
	 */
	void add_group();

	/**
	 * @brief Add a write to the group opened last.
	 *
	 * @param offset Where in the file the stretch starts, in bytes.
	 * @param buffers Where its bytes come from, in order: as many as the
	 * stretch covers.
	 * @pre A group has been opened.
	 */
	void add_write(
		std::uint64_t offset,
		span<const const_memory_range> buffers
	);

	/**
	 * @brief Get how many groups were opened.
	 *
	 * @return std::size_t The number of groups.
	 */
	std::size_t get_group_count() const noexcept;

	/**
	 * @brief Get the first write of one group.
	 *
	 * @param group Index of the group, below @ref get_group_count.
	 * @return std::size_t Its index among the writes.
	 */
	std::size_t get_first_write(std::size_t group) const noexcept;

	/**
	 * @brief Get how many writes one group holds.
	 *
	 * @param group Index of the group, below @ref get_group_count.
	 * @return std::size_t The number of writes, which may be zero.
	 */
	std::size_t get_write_count(std::size_t group) const noexcept;

	/**
	 * @brief Get how many writes there are.
	 *
	 * @return std::size_t The number of writes, in every group.
	 */
	std::size_t get_write_count() const noexcept;

	/**
	 * @brief Get the group one write falls in.
	 *
	 * @param write Index of the write, below @ref get_write_count.
	 * @return std::size_t The group.
	 */
	std::size_t get_group(std::size_t write) const noexcept;

	/**
	 * @brief Get where in the file the stretch of one write starts.
	 *
	 * @param write Index of the write, below @ref get_write_count.
	 * @return std::uint64_t The offset, in bytes.
	 */
	std::uint64_t get_offset(std::size_t write) const noexcept;

	/**
	 * @brief Get how many bytes the stretch of one write covers.
	 *
	 * @param write Index of the write, below @ref get_write_count.
	 * @return std::size_t The size in bytes, which its buffers hold.
	 */
	std::size_t get_size(std::size_t write) const noexcept;

	/**
	 * @brief Get where the bytes of one write come from.
	 *
	 * @param write Index of the write, below @ref get_write_count.
	 * @return span<const const_memory_range> Its buffers, in order. They
	 * refer to storage owned by this plan.
	 */
	span<const const_memory_range>
	get_buffers(std::size_t write) const noexcept;

private:
	struct planned_write
	{
		std::size_t group;
		std::uint64_t offset;
		std::size_t size;
		std::size_t first_buffer;
		std::size_t buffer_count;
	};

	std::vector<planned_write> m_writes;
	std::vector<const_memory_range> m_buffers;
	std::vector<std::size_t> m_first_writes;
};

/**
 * @brief Take the completion of a write of a plan.
 *
 * @param path Path to the file, which what is thrown names.
 * @param plan The writes.
 * @param completion The completion, tagged with the index of the write.
 * @return std::size_t The index of the write, whose stretch was written
 * whole.
 * @throws image_file_error If the system failed the write.
 */
std::size_t take_write(
	const std::string &path,
	const file_write_plan &plan,
	const file_transfer_completion &completion
);

} // namespace vitrio
