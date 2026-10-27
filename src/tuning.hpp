// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <boost/optional.hpp>

#include <cstddef>
#include <string>

// The numbers that tune the library, as it runs. Each is the variable of the
// environment of its name, VITRIO_STAGING_BYTE_BUDGET for one, or where that
// is not set the macro of config.hpp that names its default,
// VITRIO_DEFAULT_STAGING_BYTE_BUDGET. Each is read once, the first time it is
// asked for. A variable that does not hold a whole number above zero is
// logged, and the default taken instead.

namespace vitrio
{

/**
 * @brief Read a number that tunes the library from what a variable of the
 * environment holds.
 *
 * @param name The name of the variable, which what is logged names.
 * @param text What the variable holds, or none where it is not set.
 * @param fallback The number where the variable is not set, or does not hold
 * a whole number above zero, which is logged.
 * @return std::size_t The number.
 */
std::size_t parse_tuning_value(
	const char *name,
	const boost::optional<std::string> &text,
	std::size_t fallback
) noexcept;

/**
 * @brief Tell whether a variable of the environment holds a flag that is set:
 * anything but nothing and 0.
 *
 * @param text What the variable holds, or none where it is not set.
 * @return bool Whether the flag is set.
 */
bool parse_tuning_flag(const boost::optional<std::string> &text) noexcept;

/**
 * @brief Get the widest gap between two stretches of a file that is bridged:
 * the variable @c VITRIO_PREFETCH_GAP_CAP of the environment, or
 * @c VITRIO_DEFAULT_PREFETCH_GAP_CAP.
 *
 * @return std::size_t The gap, in bytes.
 */
std::size_t get_prefetch_gap_cap() noexcept;

/**
 * @brief Get how many bytes a read of a mapping advises at once: the
 * variable @c VITRIO_PREFETCH_BYTE_BUDGET of the environment, or
 * @c VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET.
 *
 * @return std::size_t The bytes.
 */
std::size_t get_prefetch_byte_budget() noexcept;

/**
 * @brief Get how many bytes a step of a read stages in memory of its own:
 * the variable @c VITRIO_STAGING_BYTE_BUDGET of the environment, or
 * @c VITRIO_DEFAULT_STAGING_BYTE_BUDGET.
 *
 * @return std::size_t The bytes.
 */
std::size_t get_staging_byte_budget() noexcept;

/**
 * @brief Get how many bytes the loads of a scratch kept in a file stage at
 * once: the variable @c VITRIO_SCRATCH_LOAD_BYTE_BUDGET of the environment,
 * or @c VITRIO_DEFAULT_SCRATCH_LOAD_BYTE_BUDGET.
 *
 * @return std::size_t The bytes.
 */
std::size_t get_scratch_load_byte_budget() noexcept;

/**
 * @brief Get how many reads the kernel carries out at once for one
 * asynchronous port: the variable @c VITRIO_FILE_TRANSFER_QUEUE_CAPACITY of the
 * environment, or @c VITRIO_DEFAULT_FILE_TRANSFER_QUEUE_CAPACITY.
 *
 * @return std::size_t The reads.
 */
std::size_t get_file_transfer_queue_capacity() noexcept;

/**
 * @brief Get how many threads carry out the transfers of files of a pool:
 * the variable @c VITRIO_FILE_TRANSFER_WORKERS of the environment, or
 * @c VITRIO_DEFAULT_FILE_TRANSFER_WORKERS.
 *
 * @return std::size_t The threads.
 */
std::size_t get_file_transfer_workers() noexcept;

/**
 * @brief Get how many bytes of its chunks a read of a ZARR array holds at
 * once: the variable @c VITRIO_CHUNK_BYTE_BUDGET of the environment, or
 * @c VITRIO_DEFAULT_CHUNK_BYTE_BUDGET.
 *
 * @return std::size_t The bytes.
 */
std::size_t get_chunk_byte_budget() noexcept;

/**
 * @brief Get how many chunks of a ZARR array a read has open at once: the
 * variable @c VITRIO_CHUNKS_IN_FLIGHT of the environment, or
 * @c VITRIO_DEFAULT_CHUNKS_IN_FLIGHT.
 *
 * @return std::size_t The chunks.
 */
std::size_t get_chunks_in_flight() noexcept;

/**
 * @brief Get the width of a chunk of an image of two axes written to ZARR:
 * the variable @c VITRIO_ZARR_IMAGE_CHUNK_EDGE of the environment, or
 * @c VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE.
 *
 * @return std::size_t The width.
 */
std::size_t get_zarr_image_chunk_edge() noexcept;

/**
 * @brief Get the width of a chunk of a volume written to ZARR: the variable
 * @c VITRIO_ZARR_VOLUME_CHUNK_EDGE of the environment, or
 * @c VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE.
 *
 * @return std::size_t The width.
 */
std::size_t get_zarr_volume_chunk_edge() noexcept;

/**
 * @brief Get how many bytes of chunks a writer of a ZARR array holds in
 * memory: the variable @c VITRIO_CHUNK_WRITE_BYTE_BUDGET of the environment,
 * or @c VITRIO_DEFAULT_CHUNK_WRITE_BYTE_BUDGET.
 *
 * @return std::size_t The bytes.
 */
std::size_t get_chunk_write_byte_budget() noexcept;

/**
 * @brief Tell whether files are to be transferred without asynchronous
 * ports, as where the process can not have them: whether the variable
 * @c VITRIO_DISABLE_ASYNCHRONOUS_IO of the environment is set.
 *
 * @return bool Whether they are.
 */
bool is_asynchronous_io_disabled() noexcept;

} // namespace vitrio
