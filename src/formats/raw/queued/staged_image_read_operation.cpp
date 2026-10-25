// SPDX-License-Identifier: LGPL-2.1-or-later

#include "staged_image_read_operation.hpp"

#include <strided_transfer/image_region_transfer.hpp>

#include <vitrio/concurrency/task.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <exception>
#include <utility>

namespace vitrio
{

namespace
{

// Where staged stretches start, so that the values of every data type land
// aligned in them.
constexpr std::size_t staging_alignment = 64;

// Where each stretch of a schedule is staged, within the half of the staging
// memory its step takes, and how many bytes a half takes.
struct staging_layout
{
	std::vector<std::size_t> offsets;
	std::size_t half;
};

// The first region of each stretch: the regions of a stretch follow those of
// the one before it.
std::vector<std::size_t>
find_first_regions(const image_transfer_schedule &schedule)
{
	const auto regions_per_range = schedule.get_regions_per_range();

	std::vector<std::size_t> first_regions(regions_per_range.size());
	std::size_t region = 0;
	for (std::size_t range = 0; range < first_regions.size(); ++range)
	{
		first_regions[range] = region;
		region += regions_per_range[range];
	}

	return first_regions;
}

std::vector<std::size_t>
count_regions(const image_transfer_schedule &schedule)
{
	const auto regions_per_range = schedule.get_regions_per_range();
	return std::vector<std::size_t>(
		regions_per_range.begin(),
		regions_per_range.end()
	);
}

staging_layout lay_out_staging(const image_transfer_schedule &schedule)
{
	const auto ranges = schedule.get_ranges();

	staging_layout layout{std::vector<std::size_t>(ranges.size()), 0};
	for (std::size_t step = 0; step < schedule.get_step_count(); ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();

		std::size_t offset = 0;
		for (auto range = first; range < first + count; ++range)
		{
			layout.offsets[range] = offset;
			const auto size = ranges[range].get_size();
			offset += (size + staging_alignment - 1) / staging_alignment *
				staging_alignment;
		}

		layout.half = std::max(layout.half, offset);
	}

	return layout;
}

// The reads of the stretches of a schedule into staging memory, one read a
// stretch. A step is read while the one before it is moved, so the steps
// take turns at two halves of the memory, which this sizes.
file_read_plan plan_reads(
	const image_transfer_schedule &schedule,
	std::unique_ptr<byte[]> &staging
)
{
	const auto ranges = schedule.get_ranges();
	const auto step_count = schedule.get_step_count();
	const auto layout = lay_out_staging(schedule);

	// Not filled with zeros: every byte a conversion reads is read from the
	// file first, and filling the megabytes a patch of a micrograph stages
	// took most of the time its read spent in the program, rather than in
	// the kernel, where the cache held the file.
	staging.reset(
		new byte[layout.half * std::min<std::size_t>(step_count, 2)]
	);

	file_read_plan plan;
	for (std::size_t step = 0; step < step_count; ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();
		auto *half = staging.get() + (step % 2) * layout.half;

		for (auto range = first; range < first + count; ++range)
		{
			const memory_range buffer(
				half + layout.offsets[range],
				ranges[range].get_size()
			);
			plan.add_read(
				step,
				ranges[range].get_offset(),
				make_span(&buffer, 1)
			);
		}
	}

	return plan;
}

} // anonymous namespace

// The move of one stretch into the array, which the executor of
// computations runs while the operation waits for it.
class staged_image_read_operation::conversion final
	: public task
{
public:
	conversion(
		const staged_image_read_operation &operation,
		std::size_t range
	) noexcept
		: m_operation(operation)
		, m_range(range)
	{
	}

	void run() override
	{
		m_operation.convert(m_range);
	}

private:
	const staged_image_read_operation &m_operation;
	std::size_t m_range;
};

staged_image_read_operation::staged_image_read_operation(
	std::string path,
	std::shared_ptr<const boost::interprocess::file_mapping> file,
	const raw_image_layout &layout,
	array destination,
	image_region_read_walk walk,
	const image_transfer_schedule &schedule
)
	: m_path(std::move(path))
	, m_file(std::move(file))
	, m_destination(std::move(destination))
	, m_array_data(m_destination.get_data())
	, m_array_type(m_destination.get_descriptor().get_data_type())
	, m_file_type(layout.get_descriptor().get_data_type())
	, m_file_order(layout.get_byte_order())
	, m_data_offset(layout.get_data_offset())
	, m_walk(std::move(walk))
	, m_first_regions(find_first_regions(schedule))
	, m_region_counts(count_regions(schedule))
	, m_plan(plan_reads(schedule, m_staging))
	, m_execution(m_path, m_file->get_mapping_handle().handle, m_plan)
{
}

staged_image_read_operation::~staged_image_read_operation() = default;

void staged_image_read_operation::start(file_transfer_scope &scope)
{
	m_execution.start(scope);
}

void staged_image_read_operation::on_transfer(
	const file_transfer_completion &completion,
	file_transfer_scope &scope
)
{
	const auto range = m_execution.take(completion);
	scope.run(std::make_unique<conversion>(*this, range), range);
}

void staged_image_read_operation::on_task(
	std::uint64_t tag,
	std::exception_ptr error,
	file_transfer_scope &scope
)
{
	if (error)
	{
		std::rethrow_exception(error);
	}

	m_execution.release(static_cast<std::size_t>(tag), scope);
}

void staged_image_read_operation::convert(std::size_t range) const
{
	const auto *staged = static_cast<const byte*>(
		m_plan.get_buffers(range)[0].get_address()
	);
	const auto element_size = get_size(m_file_type);
	const auto origin = (m_plan.get_offset(range) - m_data_offset) /
		element_size;

	read_regions(
		m_walk,
		m_first_regions[range],
		m_region_counts[range],
		m_array_data,
		m_array_type,
		staged,
		static_cast<std::ptrdiff_t>(origin),
		m_file_type,
		m_file_order
	);
}

} // namespace vitrio
