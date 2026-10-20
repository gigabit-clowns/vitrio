// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/image_scratch_entry.hpp>
#include <vitrio/image_scratch_open_mode.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace vitrio
{

class image_scratch_storage;

/**
 * @brief A scratch entry that keeps its copy in part of a storage.
 *
 * The entry holds some indices of the first axis of a file, for example
 * some images of a stack. Its values, an array over the storage, hold them
 * in ascending order, in the data type of the file. The position of an index
 * in that order is its slot.
 *
 * The slots are split into runs of consecutive slots. All runs have the
 * same length, except the last one, which may be shorter. A run is loaded
 * from the file in one read. Storing a region loads every run that contains
 * a held index of the region.
 *
 * A region is read from the entry only if all its indices are held and
 * loaded. A read asks the storage for the slots it is about to copy before
 * it copies them, a step ahead, as @ref read_prefetched_regions describes.
 *
 * Each run has a flag in the storage, which records that the run is loaded.
 * The flag of a loaded run holds the modification time of the file. A flag
 * that holds another time does not count, so a run loaded from an older
 * version of the file is loaded again.
 */
class indexed_image_scratch_entry final
	: public image_scratch_entry
{
public:
	/**
	 * @brief Construct an entry over part of a storage.
	 *
	 * @param indices The indices of the file to hold. They must be in
	 * ascending order and must not repeat.
	 * @param storage Where the values and the flags are kept.
	 * @param values How the block of @p storage is read as the values of the
	 * held indices. Its first extent must be the number of indices, its
	 * other extents and its data type those of the file, and its strides
	 * must not be negative.
	 * @param flags How the block of @p storage is read as one flag per run:
	 * one axis, of as many 64-bit unsigned integers as there are runs.
	 * @param modification_time When the file was last modified, or zero if
	 * that is not known.
	 * @param run_length Number of slots per run. If it exceeds the number
	 * of indices, there is a single run.
	 * @param mode Whether the entry starts empty or resumed. An empty entry
	 * clears its flags and has nothing loaded. A resumed entry has loaded
	 * the runs whose flag holds @p modification_time. If that time is zero,
	 * no run is loaded.
	 * @throws std::invalid_argument If @p storage is null, if @p indices is
	 * not strictly ascending, if @p values or @p flags is not initialized,
	 * is not aligned for its elements or does not match what it stores, if
	 * @p values has a negative stride, or if @p run_length is zero.
	 * @throws std::out_of_range If @p values or @p flags addresses an element
	 * outside the block of @p storage.
	 */
	indexed_image_scratch_entry(
		std::vector<std::size_t> indices,
		std::shared_ptr<image_scratch_storage> storage,
		array_descriptor values,
		array_descriptor flags,
		std::uint64_t modification_time,
		std::size_t run_length,
		image_scratch_open_mode mode
	);

	~indexed_image_scratch_entry() override;

	image_transfer_plan read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

	void store(
		const image_reader &file,
		const image_transfer_plan &regions
	) override;

	/**
	 * @brief Clear the flags and leave nothing loaded.
	 *
	 * Unlike the other methods, it must not be called while another method
	 * runs.
	 */
	void reset() noexcept;

private:
	/**
	 * @brief Get the flags, one per run.
	 *
	 * @return std::uint64_t* The first flag.
	 */
	std::uint64_t* get_flags() noexcept;

	/**
	 * @brief Find the slot of an index, or the slot where it would be.
	 *
	 * @param index The index.
	 * @return std::size_t The number of held indices below @p index.
	 */
	std::size_t find_slot(std::size_t index) const noexcept;

	/**
	 * @brief Check whether every slot of a range is loaded.
	 *
	 * @param first_slot First slot of the range.
	 * @param end_slot The slot after the last one of the range. Must be
	 * above @p first_slot.
	 * @return true Every run that contains a slot of the range is loaded.
	 * @return false A run that contains a slot of the range is not loaded.
	 */
	bool are_loaded(
		std::size_t first_slot,
		std::size_t end_slot
	) const noexcept;

	/**
	 * @brief Load a run from the file, unless it is already loaded.
	 *
	 * A run is loaded at most once: two threads that ask for the same run
	 * read the file once.
	 *
	 * @param file A reader over the file.
	 * @param run Index of the run.
	 */
	void load(const image_reader &file, std::size_t run);

	/**
	 * @brief Make the plan that reads a run from the file.
	 *
	 * @param run Index of the run.
	 * @return image_transfer_plan One region per slot of the run, from the
	 * index that the slot holds into the slot.
	 */
	image_transfer_plan make_load_plan(std::size_t run) const;

	std::vector<std::size_t> m_indices;
	std::size_t m_run_length;
	std::vector<std::atomic<bool>> m_loaded;
	std::shared_ptr<image_scratch_storage> m_storage;
	array m_values;
	array m_flags;
	std::uint64_t m_modification_time;
	std::mutex m_mutex;
};

} // namespace vitrio
