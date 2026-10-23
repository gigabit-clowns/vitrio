// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vitrio
{

/**
 * @brief Reads of stretches of a file, each scattered over buffers, grouped
 * into steps.
 *
 * The reads are held in the order they were added, which is that of the
 * steps they fall in.
 */
class file_read_plan
{
public:
	/**
	 * @brief Construct a plan of no read.
	 */
	file_read_plan() noexcept;

	/**
	 * @brief Add a read.
	 *
	 * @param step The step the read falls in: that of the read added before
	 * it, or a later one.
	 * @param offset Where in the file the stretch starts, in bytes.
	 * @param buffers Where its bytes go, in order: as many as the stretch
	 * covers.
	 */
	void add_read(
		std::size_t step,
		std::uint64_t offset,
		span<const memory_range> buffers
	);

	/**
	 * @brief Get how many steps the reads fall in.
	 *
	 * @return std::size_t One past the step of the last read, and zero where
	 * there is no read.
	 */
	std::size_t get_step_count() const noexcept;

	/**
	 * @brief Get how many reads there are.
	 *
	 * @return std::size_t The number of reads.
	 */
	std::size_t get_read_count() const noexcept;

	/**
	 * @brief Get the step one read falls in.
	 *
	 * @param read Index of the read, below @ref get_read_count.
	 * @return std::size_t The step.
	 */
	std::size_t get_step(std::size_t read) const noexcept;

	/**
	 * @brief Get where in the file the stretch of one read starts.
	 *
	 * @param read Index of the read, below @ref get_read_count.
	 * @return std::uint64_t The offset, in bytes.
	 */
	std::uint64_t get_offset(std::size_t read) const noexcept;

	/**
	 * @brief Get how many bytes the stretch of one read covers.
	 *
	 * @param read Index of the read, below @ref get_read_count.
	 * @return std::size_t The size in bytes, which its buffers hold.
	 */
	std::size_t get_size(std::size_t read) const noexcept;

	/**
	 * @brief Get where the bytes of one read go.
	 *
	 * @param read Index of the read, below @ref get_read_count.
	 * @return span<const memory_range> Its buffers, in order. They refer to
	 * storage owned by this plan.
	 */
	span<const memory_range> get_buffers(std::size_t read) const noexcept;

private:
	struct planned_read
	{
		std::size_t step;
		std::uint64_t offset;
		std::size_t size;
		std::size_t first_buffer;
		std::size_t buffer_count;
	};

	std::vector<planned_read> m_reads;
	std::vector<memory_range> m_buffers;
};

} // namespace vitrio
