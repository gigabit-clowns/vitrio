// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/executor_image_loader.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/counting_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_sanitizer.hpp>

#include <concurrency/failure_forwarding_notifier.hpp>
#include <image_region_grouping.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

class image_read_task final : public task
{
public:
	image_read_task(
		std::string key,
		image_transfer_plan transfer,
		std::shared_ptr<array> destination,
		std::shared_ptr<image_reader_provider> readers,
		std::shared_ptr<const image_transfer_sanitizer> sanitizer,
		std::shared_ptr<completion_notifier> done
	)
		: m_key(std::move(key))
		, m_transfer(std::move(transfer))
		, m_destination(std::move(destination))
		, m_readers(std::move(readers))
		, m_sanitizer(std::move(sanitizer))
		, m_done(std::move(done))
	{
	}

	// Opens the file and starts its reads, which report the file to the
	// batch once they all have. A failure before they start is the task's,
	// and the executor reports it.
	void run() override
	{
		const auto reader = m_readers->acquire(m_key);

		auto sanitized = m_sanitizer->sanitize(
			m_transfer,
			reader->get_descriptor().get_extents(),
			m_destination->get_descriptor().get_extents()
		);

		const auto file_done =
			std::make_shared<counting_notifier>(sanitized.size(), m_done);
		for (auto &regions : sanitized)
		{
			reader->read_async(
				m_destination->share(),
				std::move(regions),
				file_done
			);
		}
	}

private:
	std::string m_key;
	image_transfer_plan m_transfer;
	std::shared_ptr<array> m_destination;
	std::shared_ptr<image_reader_provider> m_readers;
	std::shared_ptr<const image_transfer_sanitizer> m_sanitizer;
	std::shared_ptr<completion_notifier> m_done;
};

} // anonymous namespace

executor_image_loader::executor_image_loader(
	std::shared_ptr<image_reader_provider> readers,
	std::shared_ptr<vitrio::executor> executor
)
	: m_readers(std::move(readers))
	, m_executor(std::move(executor))
{
	if (!m_readers)
	{
		throw std::invalid_argument(
			"executor_image_loader: The reader provider must not be null."
		);
	}

	if (!m_executor)
	{
		throw std::invalid_argument(
			"executor_image_loader: The executor must not be null."
		);
	}
}

executor_image_loader::~executor_image_loader() = default;

std::shared_ptr<completion> executor_image_loader::load(
	array destination,
	const image_transaction_plan &plan,
	std::shared_ptr<const image_transfer_sanitizer> sanitizer
) const
{
	if (!sanitizer)
	{
		throw std::invalid_argument(
			"executor_image_loader: The sanitizer must not be null."
		);
	}

	image_region_grouping grouping;
	grouping.build(plan);

	auto shared_destination = std::make_shared<array>(std::move(destination));
	auto result = std::make_shared<counting_completion>(
		grouping.get_addressed_file_count()
	);

	const auto file_count = grouping.get_file_count();
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		if (grouping.get_file_region_count(file_index) == 0)
		{
			continue;
		}

		m_executor->submit(
			std::make_unique<image_read_task>(
				plan.get_file(file_index),
				make_file_transfer_plan(grouping, plan, file_index),
				shared_destination,
				m_readers,
				sanitizer,
				result
			),
			std::make_shared<failure_forwarding_notifier>(result)
		);
	}

	return result;
}

} // namespace vitrio
