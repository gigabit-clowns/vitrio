// SPDX-License-Identifier: LGPL-2.1-or-later

#include "indexed_image_scratch_entry.hpp"

#include "image_scratch_storage_bounds.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>

#include <vitrio/concurrency/counting_notifier.hpp>

#include <assert.hpp>

#include <boost/optional.hpp>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

std::size_t
compute_run_count(std::size_t slot_count, std::size_t run_length)
{
	if (run_length == 0)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: A run must span at least one slot."
		);
	}

	if (slot_count == 0)
	{
		return 0;
	}

	return (slot_count - 1) / run_length + 1;
}

std::shared_ptr<image_scratch_storage>
check_storage(std::shared_ptr<image_scratch_storage> storage)
{
	if (!storage)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The storage must not be null."
		);
	}

	return storage;
}

// The bytes of a run flag, as the storage holds it.
span<const byte> as_bytes(const std::uint64_t &flag) noexcept
{
	return make_span(reinterpret_cast<const byte*>(&flag), sizeof(flag));
}

// A range that reaches past the largest index ends at it, which is an index
// that no file has.
std::size_t
compute_end_index(std::size_t first_index, std::size_t index_count) noexcept
{
	const auto max_index = std::numeric_limits<std::size_t>::max();
	return first_index + std::min(index_count, max_index - first_index);
}

} // anonymous namespace

struct indexed_image_scratch_entry::held_split
{
	image_transfer_plan held;
	image_transfer_plan missing;
};

// What the read that loads a run reports to: it settles the run. It keeps
// the entry, whose storage the read fills, until it has.
class indexed_image_scratch_entry::run_load_notifier final
	: public completion_notifier
{
public:
	run_load_notifier(
		std::shared_ptr<indexed_image_scratch_entry> entry,
		std::size_t run
	) noexcept
		: m_entry(std::move(entry))
		, m_run(run)
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		m_entry->finish_load(m_run, std::move(error));
	}

private:
	std::shared_ptr<indexed_image_scratch_entry> m_entry;
	std::size_t m_run;
};

// What the write of the flag of a run reports to: it settles the run.
class indexed_image_scratch_entry::run_flag_notifier final
	: public completion_notifier
{
public:
	run_flag_notifier(
		std::shared_ptr<indexed_image_scratch_entry> entry,
		std::size_t run
	) noexcept
		: m_entry(std::move(entry))
		, m_run(run)
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		m_entry->settle(m_run, std::move(error));
	}

private:
	std::shared_ptr<indexed_image_scratch_entry> m_entry;
	std::size_t m_run;
};

indexed_image_scratch_entry::indexed_image_scratch_entry(
	std::vector<std::size_t> indices,
	std::shared_ptr<image_scratch_storage> storage,
	array_descriptor values,
	array_descriptor flags,
	std::uint64_t modification_time,
	std::size_t run_length,
	image_scratch_open_mode mode
)
	: m_indices(std::move(indices))
	, m_run_length(run_length)
	, m_loaded(compute_run_count(m_indices.size(), run_length))
	, m_storage(check_storage(std::move(storage)))
	, m_values(std::move(values))
	, m_flags_offset(
		static_cast<std::size_t>(flags.get_offset()) * sizeof(std::uint64_t)
	)
	, m_modification_time(modification_time)
	, m_waiters(m_loaded.size())
{
	const auto unordered = std::adjacent_find(
		m_indices.begin(),
		m_indices.end(),
		std::greater_equal<std::size_t>()
	);
	if (unordered != m_indices.end())
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The indices are not strictly "
			"ascending."
		);
	}

	const auto strides = m_values.get_strides();
	const auto descends = std::any_of(
		strides.begin(),
		strides.end(),
		[](std::ptrdiff_t stride) { return stride < 0; }
	);
	if (descends)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The values have a negative stride."
		);
	}

	const auto extents = m_values.get_extents();
	if (extents.empty() || extents.front() != m_indices.size())
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The first extent of the values is "
			"not the number of indices."
		);
	}

	const auto flag_extents = flags.get_extents();
	if (
		flags.get_data_type() != numerical_type::uint64 ||
		flag_extents.size() != 1 ||
		flag_extents.front() != m_loaded.size()
	)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The flags are not one 64-bit "
			"unsigned integer per run."
		);
	}

	const auto size = m_storage->get_size();
	check_values_within(m_values, size, "indexed_image_scratch_entry");
	check_values_within(flags, size, "indexed_image_scratch_entry");

	if (mode == image_scratch_open_mode::empty)
	{
		reset();
		return;
	}

	std::vector<std::uint64_t> run_flags(m_loaded.size());
	m_storage->read(
		m_flags_offset,
		make_span(
			reinterpret_cast<byte*>(run_flags.data()),
			run_flags.size() * sizeof(std::uint64_t)
		)
	);
	for (std::size_t run = 0; run < m_loaded.size(); ++run)
	{
		const auto is_loaded =
			m_modification_time != 0 &&
			run_flags[run] == m_modification_time;
		m_loaded[run].store(is_loaded, std::memory_order_relaxed);
	}
}

indexed_image_scratch_entry::~indexed_image_scratch_entry() = default;

image_transfer_plan indexed_image_scratch_entry::read_async(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	if (!done)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The notifier must not be null."
		);
	}

	if (regions.get_shape().get_file_rank() != m_values.get_extents().size())
	{
		done->notify_done();
		return regions;
	}

	// Nothing is read when the regions can not be told apart, so every one
	// of them is answered.
	boost::optional<held_split> split;
	try
	{
		split = split_held(regions);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return regions;
	}

	if (split->held.get_region_count() == 0)
	{
		done->notify_done();
		return std::move(split->missing);
	}

	m_storage->serve_async(
		m_values,
		std::move(destination),
		std::move(split->held),
		std::move(done)
	);
	return std::move(split->missing);
}

void indexed_image_scratch_entry::store_async(
	std::shared_ptr<const image_reader> file,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
)
{
	if (!done)
	{
		throw std::invalid_argument(
			"indexed_image_scratch_entry: The notifier must not be null."
		);
	}

	// Each run is reported to the store once, whether it is held already,
	// loads here, or loads for another store. What is allocated is
	// allocated before the lock, so that taking a run never fails under it.
	std::vector<std::size_t> runs;
	std::shared_ptr<completion_notifier> waiting;
	std::vector<waiter_list> joining;
	std::vector<std::size_t> started;
	try
	{
		if (!file)
		{
			throw std::invalid_argument(
				"indexed_image_scratch_entry: The file must not be null."
			);
		}
		runs = find_runs(regions);
		waiting = std::make_shared<counting_notifier>(runs.size(), done);
		joining.resize(runs.size());
		for (auto &join : joining)
		{
			join.push_back(waiting);
		}
		started.reserve(runs.size());
	}
	catch (...)
	{
		if (!waiting)
		{
			done->notify_done(std::current_exception());
		}
		else
		{
			// The count may stand already, and reports for every run.
			for (std::size_t run = 0; run < runs.size(); ++run)
			{
				waiting->notify_done(std::current_exception());
			}
		}
		return;
	}

	std::size_t held = 0;
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		for (std::size_t index = 0; index < runs.size(); ++index)
		{
			const auto run = runs[index];
			if (m_loaded[run].load(std::memory_order_relaxed))
			{
				++held;
				continue;
			}

			// A run with no waiter is absent, and this store loads it.
			auto &waiters = m_waiters[run];
			if (waiters.empty())
			{
				started.push_back(run);
			}
			waiters.splice(waiters.end(), joining[index]);
		}
	}

	for (std::size_t count = 0; count < held; ++count)
	{
		waiting->notify_done();
	}

	for (const auto run : started)
	{
		start_load(file, run);
	}
}

void indexed_image_scratch_entry::reset()
{
	for (std::size_t run = 0; run < m_loaded.size(); ++run)
	{
		VITRIO_ASSERT(m_waiters[run].empty());
		m_loaded[run].store(false, std::memory_order_relaxed);
	}

	const std::vector<std::uint64_t> run_flags(m_loaded.size(), 0);
	m_storage->write(
		m_flags_offset,
		make_span(
			reinterpret_cast<const byte*>(run_flags.data()),
			run_flags.size() * sizeof(std::uint64_t)
		)
	);
}

std::size_t
indexed_image_scratch_entry::find_slot(std::size_t index) const noexcept
{
	const auto ite =
		std::lower_bound(m_indices.begin(), m_indices.end(), index);
	return static_cast<std::size_t>(std::distance(m_indices.begin(), ite));
}

bool indexed_image_scratch_entry::are_loaded(
	std::size_t first_slot,
	std::size_t end_slot
) const noexcept
{
	VITRIO_ASSERT(first_slot < end_slot);
	VITRIO_ASSERT(end_slot <= m_indices.size());

	const auto last_run = (end_slot - 1) / m_run_length;
	for (auto run = first_slot / m_run_length; run <= last_run; ++run)
	{
		if (!m_loaded[run].load(std::memory_order_acquire))
		{
			return false;
		}
	}

	return true;
}

indexed_image_scratch_entry::held_split
indexed_image_scratch_entry::split_held(
	const image_transfer_plan &regions
) const
{
	const auto &shape = regions.get_shape();
	const auto rank = m_values.get_extents().size();
	VITRIO_ASSERT(shape.get_file_rank() == rank);

	held_split split{image_transfer_plan(shape), image_transfer_plan(shape)};
	std::vector<std::size_t> slot_offset(rank);

	const auto index_count = shape.get_extent(rank, 0);
	const auto region_count = regions.get_region_count();
	for (std::size_t region = 0; region < region_count; ++region)
	{
		const auto file_offset = regions.get_file_offset(region);
		const auto array_offset = regions.get_array_offset(region);

		const auto first_index = file_offset.front();
		const auto first_slot = find_slot(first_index);
		const auto end_slot =
			find_slot(compute_end_index(first_index, index_count));
		const auto is_held =
			index_count > 0 && end_slot - first_slot == index_count;
		if (!is_held || !are_loaded(first_slot, end_slot))
		{
			split.missing.add(file_offset, array_offset);
			continue;
		}

		slot_offset.assign(file_offset.begin(), file_offset.end());
		slot_offset.front() = first_slot;
		split.held.add(make_span(slot_offset), array_offset);
	}

	return split;
}

std::vector<std::size_t> indexed_image_scratch_entry::find_runs(
	const image_transfer_plan &regions
) const
{
	std::vector<std::size_t> runs;
	const auto &shape = regions.get_shape();
	const auto rank = m_values.get_extents().size();
	if (shape.get_file_rank() != rank)
	{
		return runs;
	}

	const auto index_count = shape.get_extent(rank, 0);
	const auto region_count = regions.get_region_count();
	for (std::size_t region = 0; region < region_count; ++region)
	{
		const auto first_index = regions.get_file_offset(region).front();
		const auto first_slot = find_slot(first_index);
		const auto end_slot =
			find_slot(compute_end_index(first_index, index_count));
		if (first_slot == end_slot)
		{
			continue;
		}

		const auto last_run = (end_slot - 1) / m_run_length;
		for (auto run = first_slot / m_run_length; run <= last_run; ++run)
		{
			runs.push_back(run);
		}
	}

	std::sort(runs.begin(), runs.end());
	runs.erase(std::unique(runs.begin(), runs.end()), runs.end());
	return runs;
}

void indexed_image_scratch_entry::start_load(
	const std::shared_ptr<const image_reader> &file,
	std::size_t run
) noexcept
{
	VITRIO_ASSERT(run < m_loaded.size());

	try
	{
		m_storage->load_async(
			file,
			m_values,
			make_load_plan(run),
			std::make_shared<run_load_notifier>(shared_from_this(), run)
		);
	}
	catch (...)
	{
		finish_load(run, std::current_exception());
	}
}

void indexed_image_scratch_entry::finish_load(
	std::size_t run,
	std::exception_ptr error
)
{
	// The flag is written as a transfer, since the read may report on a
	// thread that carries out transfers, which a write that blocks would
	// hold up. A run whose flag can not be written is not held.
	if (!error)
	{
		try
		{
			m_storage->write_async(
				m_flags_offset + run * sizeof(std::uint64_t),
				as_bytes(m_modification_time),
				std::make_shared<run_flag_notifier>(shared_from_this(), run)
			);
			return;
		}
		catch (...)
		{
			error = std::current_exception();
		}
	}

	settle(run, std::move(error));
}

void indexed_image_scratch_entry::settle(
	std::size_t run,
	std::exception_ptr error
)
{
	waiter_list waiters;
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (!error)
		{
			m_loaded[run].store(true, std::memory_order_release);
		}
		waiters.swap(m_waiters[run]);
	}

	for (const auto &waiter : waiters)
	{
		waiter->notify_done(error);
	}
}

image_transfer_plan
indexed_image_scratch_entry::make_load_plan(std::size_t run) const
{
	// The first axis counts the images held. The rest are those of one.
	const auto held = m_values.get_extents();
	const auto rank = held.size();
	std::vector<std::size_t> extents(held.begin() + 1, held.end());
	image_transfer_plan plan(
		image_transfer_shape(std::move(extents), rank, rank)
	);

	const auto first_slot = run * m_run_length;
	const auto slot_count =
		std::min(m_run_length, m_indices.size() - first_slot);
	plan.reserve(slot_count);

	std::vector<std::size_t> file_offset(rank, 0);
	std::vector<std::size_t> array_offset(rank, 0);
	for (std::size_t slot = first_slot; slot < first_slot + slot_count; ++slot)
	{
		file_offset.front() = m_indices[slot];
		array_offset.front() = slot;
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

} // namespace vitrio
