// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scratch_image_reader.hpp"

#include <vitrio/concurrency/counting_notifier.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_scratch_entry.hpp>

#include <concurrency/failure_forwarding_notifier.hpp>

#include <assert.hpp>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

// What a read does once the entry has taken in what it was offered: read
// from the entry what it now holds, and from the file whatever it does not,
// which report as one.
class held_read final
	: public task
{
public:
	held_read(
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<const image_reader> file,
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) noexcept
		: m_entry(std::move(entry))
		, m_file(std::move(file))
		, m_destination(std::move(destination))
		, m_regions(std::move(regions))
		, m_done(std::move(done))
	{
	}

	void run() override
	{
		const auto both = std::make_shared<counting_notifier>(2, m_done);
		const auto unheld = m_entry->read_async(
			m_destination.share(),
			std::move(m_regions),
			both
		);
		if (unheld.get_region_count() == 0)
		{
			both->notify_done();
			return;
		}

		m_file->read_async(std::move(m_destination), unheld, both);
	}

private:
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<const image_reader> m_file;
	array m_destination;
	image_transfer_plan m_regions;
	std::shared_ptr<completion_notifier> m_done;
};

// What the entry reports to once it has taken in the regions it was offered:
// a failure is the read's, and otherwise a task reads them from it. The
// task reports only its own failure, since the read it finishes reports.
class scratch_read_continuation final
	: public completion_notifier
{
public:
	scratch_read_continuation(
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<const image_reader> file,
		std::shared_ptr<executor> tasks,
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) noexcept
		: m_entry(std::move(entry))
		, m_file(std::move(file))
		, m_tasks(std::move(tasks))
		, m_destination(std::move(destination))
		, m_regions(std::move(regions))
		, m_done(std::move(done))
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		if (error)
		{
			m_done->notify_done(std::move(error));
			return;
		}

		const auto reported =
			std::make_shared<failure_forwarding_notifier>(m_done);
		m_tasks->submit(
			std::make_unique<held_read>(
				std::move(m_entry),
				std::move(m_file),
				std::move(m_destination),
				std::move(m_regions),
				std::move(m_done)
			),
			reported
		);
	}

private:
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<const image_reader> m_file;
	std::shared_ptr<executor> m_tasks;
	array m_destination;
	image_transfer_plan m_regions;
	std::shared_ptr<completion_notifier> m_done;
};

// One part of a read split: read as a whole read of its own, by a reader
// over the same entry and file, and reported to what gathers the parts.
// Whoever started the read may let its reader go before the task runs, so
// the task holds what a reader is made of. It reports only its own
// failure, since the read it starts reports.
class part_read final
	: public task
{
public:
	part_read(
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<const image_reader> file,
		std::shared_ptr<executor> tasks,
		std::size_t task_bytes,
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) noexcept
		: m_entry(std::move(entry))
		, m_file(std::move(file))
		, m_tasks(std::move(tasks))
		, m_task_bytes(task_bytes)
		, m_destination(std::move(destination))
		, m_regions(std::move(regions))
		, m_done(std::move(done))
	{
	}

	void run() override
	{
		const scratch_image_reader reader(
			std::move(m_entry),
			std::move(m_file),
			std::move(m_tasks),
			m_task_bytes
		);
		reader.read_async(
			std::move(m_destination),
			std::move(m_regions),
			std::move(m_done)
		);
	}

private:
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<const image_reader> m_file;
	std::shared_ptr<executor> m_tasks;
	std::size_t m_task_bytes;
	array m_destination;
	image_transfer_plan m_regions;
	std::shared_ptr<completion_notifier> m_done;
};

// The bytes one region of a plan moves into the array.
std::size_t get_region_bytes(
	const image_transfer_plan &regions,
	const array &destination
)
{
	std::size_t bytes = get_size(destination.get_descriptor().get_data_type());
	for (const auto extent : regions.get_shape().get_extents())
	{
		bytes *= extent;
	}
	return bytes;
}

// The regions `from` on, `count` of them, as a plan of their own.
image_transfer_plan take_regions(
	const image_transfer_plan &regions,
	std::size_t from,
	std::size_t count
)
{
	image_transfer_plan part(regions.get_shape());
	part.reserve(count);
	for (std::size_t region = from; region < from + count; ++region)
	{
		part.add(
			regions.get_file_offset(region),
			regions.get_array_offset(region)
		);
	}
	return part;
}

} // anonymous namespace

scratch_image_reader::scratch_image_reader(
	std::shared_ptr<image_scratch_entry> entry,
	std::shared_ptr<const image_reader> file,
	std::shared_ptr<executor> tasks,
	std::size_t task_bytes
)
	: m_entry(std::move(entry))
	, m_file(std::move(file))
	, m_tasks(std::move(tasks))
	, m_task_bytes(task_bytes)
{
	VITRIO_ASSERT(m_entry);
	VITRIO_ASSERT(m_file);
	VITRIO_ASSERT(m_tasks);
}

scratch_image_reader::~scratch_image_reader() = default;

const image_descriptor& scratch_image_reader::get_descriptor() const noexcept
{
	return m_file->get_descriptor();
}

const image_metadata& scratch_image_reader::get_metadata() const noexcept
{
	return m_file->get_metadata();
}

void scratch_image_reader::read_async(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	if (!done)
	{
		throw std::invalid_argument(
			"scratch_image_reader: The notifier must not be null."
		);
	}

	// A read of more than a task copies is split into reads of that many
	// regions, a task each, which report as one. The parts are the regions
	// in their order, so a part of the batch of a loader is a run of it.
	const auto count = regions.get_region_count();
	const auto per_part = std::max<std::size_t>(
		1, m_task_bytes / std::max<std::size_t>(
			1, get_region_bytes(regions, destination)));
	if (count > per_part)
	{
		const auto parts = (count + per_part - 1) / per_part;
		std::shared_ptr<completion_notifier> all;
		try
		{
			all = std::make_shared<counting_notifier>(parts, done);
		}
		catch (...)
		{
			done->notify_done(std::current_exception());
			return;
		}

		for (std::size_t from = 0; from < count; from += per_part)
		{
			const auto reported =
				std::make_shared<failure_forwarding_notifier>(all);
			m_tasks->submit(
				std::make_unique<part_read>(
					m_entry,
					m_file,
					m_tasks,
					m_task_bytes,
					destination.share(),
					take_regions(
						regions, from, std::min(per_part, count - from)),
					all
				),
				reported
			);
		}
		return;
	}

	read_part(std::move(destination), std::move(regions), std::move(done));
}

void scratch_image_reader::read_part(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	// What the entry holds is read from it, and what it does not is offered
	// to it and read once it has taken that in. The two report as one, and
	// each reports what fails it.
	std::shared_ptr<completion_notifier> both;
	try
	{
		both = std::make_shared<counting_notifier>(2, done);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	auto missing = m_entry->read_async(
		destination.share(),
		std::move(regions),
		both
	);
	if (missing.get_region_count() == 0)
	{
		both->notify_done();
		return;
	}

	std::shared_ptr<completion_notifier> continuation;
	try
	{
		continuation = std::make_shared<scratch_read_continuation>(
			m_entry,
			m_file,
			m_tasks,
			std::move(destination),
			missing,
			both
		);
	}
	catch (...)
	{
		both->notify_done(std::current_exception());
		return;
	}

	m_entry->store_async(m_file, std::move(missing), std::move(continuation));
}

} // namespace vitrio
