// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/executor_image_saver.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_sanitizer.hpp>
#include <vitrio/image_writer.hpp>
#include <vitrio/image_writer_provider.hpp>

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

class image_write_task final : public task
{
public:
	image_write_task(
		std::string path,
		image_transfer_plan transfer,
		std::shared_ptr<const_array> source,
		std::shared_ptr<image_writer_provider> writers,
		std::shared_ptr<const image_transfer_sanitizer> sanitizer
	)
		: m_path(std::move(path))
		, m_transfer(std::move(transfer))
		, m_source(std::move(source))
		, m_writers(std::move(writers))
		, m_sanitizer(std::move(sanitizer))
	{
	}

	void run() override
	{
		const auto writer = m_writers->acquire(m_path);
		const_array_ref source(*m_source);

		const auto sanitized = m_sanitizer->sanitize(
			m_transfer,
			writer->get_descriptor().get_extents(),
			source.get_descriptor().get_extents()
		);
		for (const auto &regions : sanitized)
		{
			writer->write(source, regions);
		}
	}

private:
	std::string m_path;
	image_transfer_plan m_transfer;
	std::shared_ptr<const_array> m_source;
	std::shared_ptr<image_writer_provider> m_writers;
	std::shared_ptr<const image_transfer_sanitizer> m_sanitizer;
};

} // anonymous namespace

executor_image_saver::executor_image_saver(
	std::shared_ptr<image_writer_provider> writers,
	std::shared_ptr<vitrio::executor> executor
)
	: m_writers(std::move(writers))
	, m_executor(std::move(executor))
{
	if (!m_writers)
	{
		throw std::invalid_argument(
			"executor_image_saver: The writer provider must not be null."
		);
	}

	if (!m_executor)
	{
		throw std::invalid_argument(
			"executor_image_saver: The executor must not be null."
		);
	}
}

executor_image_saver::~executor_image_saver() = default;

std::shared_ptr<completion> executor_image_saver::save(
	const_array source,
	const image_transaction_plan &plan,
	std::shared_ptr<const image_transfer_sanitizer> sanitizer
) const
{
	if (!sanitizer)
	{
		throw std::invalid_argument(
			"executor_image_saver: The sanitizer must not be null."
		);
	}

	image_region_grouping grouping;
	grouping.build(plan);

	auto shared_source = std::make_shared<const_array>(std::move(source));
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
			std::make_unique<image_write_task>(
				plan.get_file(file_index),
				make_file_transfer_plan(grouping, plan, file_index),
				shared_source,
				m_writers,
				sanitizer
			),
			result
		);
	}

	return result;
}

} // namespace vitrio
