// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/image_scratch_entry.hpp>
#include <vitrio/image_scratch_open_mode.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <list>
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
 * some images of a stack. Its values, which the storage holds as an array
 * descriptor states, hold them in ascending order, in the data type of the
 * file. The position of an index in that order is its slot.
 *
 * The slots are split into runs of consecutive slots. All runs have the
 * same length, except the last one, which may be shorter. A run is loaded
 * from the file in one read. Storing a region loads every run that contains
 * a held index of the region.
 *
 * A run is absent, loading or loaded. One that is loading has waiters, the
 * stores that asked for it, which are reported to once its read lands; one
 * that a store asks for while it is loading is waited for, not read again.
 * A run whose read fails is absent again. A short lock guards the waiters,
 * and never a read.
 *
 * A region is read from the entry only if all its indices are held and
 * loaded. The storage serves it, and takes in the runs a store loads.
 *
 * Each run has a flag in the storage, which records that the run is loaded.
 * The flag of a loaded run holds the modification time of the file. A flag
 * that holds another time does not count, so a run loaded from an older
 * version of the file is loaded again.
 */
class indexed_image_scratch_entry final
	: public image_scratch_entry
	, public std::enable_shared_from_this<indexed_image_scratch_entry>
{
public:
	/**
	 * @brief Construct an entry over part of a storage.
	 *
	 * @param indices The indices of the file to hold. They must be in
	 * ascending order and must not repeat.
	 * @param storage Where the values and the flags are kept.
	 * @param values Where the block of @p storage holds the values of the
	 * held indices. Its first extent must be the number of indices, its
	 * other extents and its data type those of the file, and its strides
	 * must not be negative.
	 * @param flags Where the block of @p storage holds one flag per run:
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
	 * not strictly ascending, if @p values or @p flags does not match what
	 * it stores, if @p values has a negative stride, or if @p run_length is
	 * zero.
	 * @throws std::out_of_range If the values or the flags lie outside the
	 * block of @p storage.
	 * @throws image_file_error If the flags can not be read or, for an
	 * empty entry, written.
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

	image_transfer_plan read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const override;

	/**
	 * @copydoc image_scratch_entry::store_async
	 *
	 * The entry must be owned by a std::shared_ptr, which the reads it starts
	 * keep.
	 */
	void store_async(
		std::shared_ptr<const image_reader> file,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) override;

	/**
	 * @brief Clear the flags and leave nothing loaded.
	 *
	 * Unlike the other methods, it must not be called while another method
	 * runs, nor while a run is loading.
	 *
	 * @throws image_file_error If the flags can not be written. Nothing is
	 * loaded then either.
	 */
	void reset();

private:
	class run_load_notifier;
	struct held_split;

	using waiter_list = std::list<std::shared_ptr<completion_notifier>>;

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
	 * @brief Tell the regions of a plan that are held from those that are
	 * not.
	 *
	 * @param regions The regions. Of the rank of the file.
	 * @return held_split The regions that are held, at their slots among
	 * the values, and those that are not, as they are in @p regions.
	 */
	held_split split_held(const image_transfer_plan &regions) const;

	/**
	 * @brief Find the runs that contain a held index of the regions of a
	 * plan.
	 *
	 * @param regions The regions.
	 * @return std::vector<std::size_t> The runs, in ascending order, each
	 * once. None for a plan that does not have the rank of the file.
	 */
	std::vector<std::size_t> find_runs(
		const image_transfer_plan &regions
	) const;

	/**
	 * @brief Start the read that loads a run.
	 *
	 * @param file A reader over the file.
	 * @param run Index of the run, which is loading.
	 */
	void start_load(
		const std::shared_ptr<const image_reader> &file,
		std::size_t run
	) noexcept;

	/**
	 * @brief Settle a run whose read reported: it is loaded, or absent
	 * again if the read failed, and its waiters are told so.
	 *
	 * @param run Index of the run.
	 * @param error What the read failed with, or null.
	 */
	void finish_load(std::size_t run, std::exception_ptr error);

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

	// Where the storage holds the values, and the flags, in bytes from its
	// first.
	array_descriptor m_values;
	std::size_t m_flags_offset;

	std::uint64_t m_modification_time;

	// The stores waiting for each run, of which only a loading run has any.
	std::vector<waiter_list> m_waiters;
	std::mutex m_mutex;
};

} // namespace vitrio
