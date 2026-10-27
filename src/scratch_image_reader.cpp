// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scratch_image_reader.hpp"

#include <vitrio/concurrency/counting_notifier.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/image_scratch_entry.hpp>

#include <concurrency/failure_forwarding_notifier.hpp>

#include <assert.hpp>

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

} // anonymous namespace

scratch_image_reader::scratch_image_reader(
	std::shared_ptr<image_scratch_entry> entry,
	std::shared_ptr<const image_reader> file,
	std::shared_ptr<executor> tasks
)
	: m_entry(std::move(entry))
	, m_file(std::move(file))
	, m_tasks(std::move(tasks))
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
