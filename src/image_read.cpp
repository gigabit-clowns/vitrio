// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_read.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/clipping_image_transfer_sanitizer.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_loader.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/image_scratch.hpp>
#include <vitrio/image_scratch_entry.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/index_table.hpp>
#include <vitrio/span.hpp>
#include <vitrio/strict_image_transfer_sanitizer.hpp>

#include <array/borrowed_array.hpp>
#include <concurrency/failure_forwarding_notifier.hpp>

#include <assert.hpp>

#include <image_plan_builders.hpp>
#include <waiting_transfers.hpp>

#include <algorithm>
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

image_transfer_plan make_indices_plan(
	const image_descriptor &file,
	span<const std::size_t> indices
)
{
	const auto extents = file.get_extents();
	const auto rank = extents.size();

	image_transfer_plan plan(
		image_transfer_shape(
			std::vector<std::size_t>(extents.begin() + 1, extents.end()),
			rank,
			rank - 1
		)
	);
	plan.reserve(indices.size());

	std::vector<std::size_t> file_offset(rank, 0);
	const std::vector<std::size_t> array_offset(rank - 1, 0);
	for (const auto index : indices)
	{
		file_offset.front() = index;
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

class scratch_prefetch_task final : public task
{
public:
	scratch_prefetch_task(
		const image_location_grouping::group &group,
		std::shared_ptr<image_scratch_entry> entry,
		std::shared_ptr<image_reader_provider> files,
		std::shared_ptr<completion_notifier> done
	)
		: m_key(group.get_key())
		, m_indices(group.get_indices().begin(), group.get_indices().end())
		, m_whole(group.is_whole())
		, m_entry(std::move(entry))
		, m_files(std::move(files))
		, m_done(std::move(done))
	{
	}

	void run() override
	{
		const auto file = m_files->acquire(m_key);
		VITRIO_ASSERT(file);

		const auto &descriptor = file->get_descriptor();
		auto plan = m_whole
			? make_location_plan(descriptor, image_location(m_key))
			: make_indices_plan(descriptor, make_span(m_indices));

		// The entry reports once what it takes in has landed, with every run
		// of the file read at once.
		m_entry->store_async(*file, std::move(plan), std::move(m_done));
	}

private:
	std::string m_key;
	std::vector<std::size_t> m_indices;
	bool m_whole;
	std::shared_ptr<image_scratch_entry> m_entry;
	std::shared_ptr<image_reader_provider> m_files;
	std::shared_ptr<completion_notifier> m_done;
};

} // anonymous namespace

array read(
	const std::string &key,
	image_reader_provider &readers,
	numerical_type data_type
)
{
	return read(image_location(key), readers, data_type);
}

array read(
	const image_location &location,
	image_reader_provider &readers,
	numerical_type data_type
)
{
	const auto reader = readers.acquire(location.get_key());
	const auto &descriptor = reader->get_descriptor();
	const auto plan = make_location_plan(descriptor, location);

	auto destination = make_array(
		make_contiguous_array_descriptor(
			plan.get_shape().get_extents(),
			data_type == numerical_type::unknown
				? descriptor.get_data_type()
				: data_type
		)
	);

	read_and_wait(*reader, destination.share(), plan);

	return destination;
}

void read(
	array_ref destination,
	const image_location &location,
	image_reader_provider &readers
)
{
	const auto reader = readers.acquire(location.get_key());
	const auto plan = make_location_plan(reader->get_descriptor(), location);

	const auto extents = destination.get_descriptor().get_extents();
	const auto read_extents = plan.get_shape().get_extents();
	if (!std::equal(
			extents.begin(),
			extents.end(),
			read_extents.begin(),
			read_extents.end()
		))
	{
		throw std::invalid_argument(
			"read: The extents of the array are not those of what the "
			"location names."
		);
	}

	read_and_wait(*reader, borrow_array(destination), plan);
}

std::shared_ptr<completion> read_batch_async(
	const image_loader &loader,
	array destination,
	span<const image_location> locations
)
{
	const auto transaction = make_batch_plan(
		destination.get_descriptor().get_extents(),
		locations
	);

	return loader.load(
		std::move(destination),
		transaction,
		strict_image_transfer_sanitizer::get_shared()
	);
}

std::shared_ptr<completion> read_patches_async(
	const image_loader &loader,
	array destination,
	const image_location &location,
	const index_table &centres
)
{
	const auto transaction = make_patch_plan(
		destination.get_descriptor().get_extents(),
		location,
		centres
	);

	return loader.load(
		std::move(destination),
		transaction,
		clipping_image_transfer_sanitizer::get_shared()
	);
}

std::shared_ptr<completion> prefetch_scratch_async(
	image_scratch &scratch,
	std::shared_ptr<image_reader_provider> files,
	vitrio::executor &executor,
	const image_location_grouping &locations
)
{
	if (!files)
	{
		throw std::invalid_argument(
			"prefetch_scratch_async: The reader provider must not be null."
		);
	}

	const auto group_count = locations.get_group_count();

	std::vector<std::shared_ptr<image_scratch_entry>> entries;
	std::vector<std::size_t> groups;
	for (std::size_t index = 0; index < group_count; ++index)
	{
		auto entry = scratch.find(locations.get_group(index).get_key());
		if (entry)
		{
			entries.push_back(std::move(entry));
			groups.push_back(index);
		}
	}

	// A task reports only its own failure: the store it starts reports the
	// file.
	auto result = std::make_shared<counting_completion>(entries.size());
	const auto failures = std::make_shared<failure_forwarding_notifier>(result);
	for (std::size_t file = 0; file < entries.size(); ++file)
	{
		executor.submit(
			std::make_unique<scratch_prefetch_task>(
				locations.get_group(groups[file]),
				std::move(entries[file]),
				files,
				result
			),
			failures
		);
	}

	return result;
}

} // namespace vitrio
