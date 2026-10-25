// SPDX-License-Identifier: LGPL-2.1-or-later

#include "staged_image_write_operation.hpp"

#include <strided_transfer/image_region_transfer.hpp>

#include <vitrio/concurrency/task.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/span.hpp>

#include <assert.hpp>

#include <algorithm>
#include <exception>
#include <utility>

namespace vitrio
{

namespace
{

// Where staged stretches start, so that the values of every data type lie
// aligned in them.
constexpr std::size_t staging_alignment = 64;

std::size_t align_up(std::size_t size) noexcept
{
	return (size + staging_alignment - 1) / staging_alignment *
		staging_alignment;
}

// The stretch of the file a run covers, from its first byte to the one past
// its last.
struct covered_stretch
{
	std::size_t begin;
	std::size_t end;
};

} // anonymous namespace

// The move of one stretch out of the array into its staging, which the
// executor of computations runs while the operation waits for it.
class staged_image_write_operation::conversion final
	: public task
{
public:
	conversion(
		const staged_image_write_operation &operation,
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
	const staged_image_write_operation &m_operation;
	std::size_t m_range;
};

staged_image_write_operation::staged_image_write_operation(
	std::shared_ptr<writable_image_file> file,
	const raw_image_layout &layout,
	const_array source,
	image_region_write_walk walk,
	const image_transfer_schedule &schedule,
	const image_region_runs &runs
)
	: m_file(std::move(file))
	, m_source(std::move(source))
	, m_array_data(m_source.get_data())
	, m_array_type(m_source.get_descriptor().get_data_type())
	, m_file_type(layout.get_descriptor().get_data_type())
	, m_file_order(layout.get_byte_order())
	, m_data_offset(layout.get_data_offset())
	, m_walk(std::move(walk))
	, m_sent_steps(0)
	, m_done_steps(0)
{
	VITRIO_ASSERT(m_file);

	const auto ranges = schedule.get_ranges();
	const auto regions_per_range = schedule.get_regions_per_range();
	const auto step_count = schedule.get_step_count();

	// A step is converted while the one before it is written, so the steps
	// take turns at two halves of the staging, each as large as the largest
	// step.
	std::vector<std::size_t> offsets_in_half(ranges.size());
	std::size_t half = 0;
	m_range_steps.resize(ranges.size());
	for (std::size_t step = 0; step < step_count; ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();
		m_step_first_ranges.push_back(first);

		std::size_t offset = 0;
		for (auto range = first; range < first + count; ++range)
		{
			offsets_in_half[range] = offset;
			offset += align_up(ranges[range].get_size());
			m_range_steps[range] = step;
		}
		half = std::max(half, offset);
	}
	m_step_first_ranges.push_back(ranges.size());
	m_staging.resize(half * std::min<std::size_t>(step_count, 2));

	const auto element_size = get_size(m_file_type);
	const auto file_offsets = m_walk.get_offsets().get_file();
	const auto run_size = runs.get_run_size();
	const auto run_offsets = runs.get_run_offsets();

	std::size_t region = 0;
	std::vector<covered_stretch> covered;
	for (std::size_t range = 0; range < ranges.size(); ++range)
	{
		const auto range_offset = ranges[range].get_offset();
		auto *staged = m_staging.data() +
			(m_range_steps[range] % 2) * half + offsets_in_half[range];

		m_range_offsets.push_back(range_offset);
		m_first_regions.push_back(region);
		m_region_counts.push_back(regions_per_range[range]);
		m_staged.push_back(staged);

		// The runs the regions of the stretch cover, merged where they
		// touch: the regions of a stretch may interleave, as patches side by
		// side do.
		covered.clear();
		const auto end_region = region + regions_per_range[range];
		for (; region < end_region; ++region)
		{
			const auto start = m_data_offset +
				static_cast<std::size_t>(file_offsets[region]) * element_size;
			for (const auto run_offset : run_offsets)
			{
				covered.push_back(
					{start + run_offset, start + run_offset + run_size}
				);
			}
		}
		std::sort(
			covered.begin(),
			covered.end(),
			[](const covered_stretch &a, const covered_stretch &b)
			{
				return a.begin < b.begin;
			}
		);

		m_plan.add_group();
		for (std::size_t first = 0; first < covered.size();)
		{
			auto merged = covered[first];
			auto next = first + 1;
			while (next < covered.size() && covered[next].begin <= merged.end)
			{
				merged.end = std::max(merged.end, covered[next].end);
				++next;
			}

			VITRIO_ASSERT(merged.begin >= range_offset);
			const const_memory_range buffer(
				staged + (merged.begin - range_offset),
				merged.end - merged.begin
			);
			m_plan.add_write(merged.begin, make_span(&buffer, 1));
			first = next;
		}
	}

	m_pending.assign(step_count, 0);
}

staged_image_write_operation::~staged_image_write_operation() = default;

void staged_image_write_operation::start(file_transfer_scope &scope)
{
	send_ahead(scope);
}

void staged_image_write_operation::on_transfer(
	const file_transfer_completion &completion,
	file_transfer_scope &scope
)
{
	const auto write = take_write(m_file->get_path(), m_plan, completion);
	const auto step = m_range_steps[m_plan.get_group(write)];
	VITRIO_ASSERT(m_pending[step] > 0);

	--m_pending[step];
	send_ahead(scope);
}

void staged_image_write_operation::on_task(
	std::uint64_t tag,
	std::exception_ptr error,
	file_transfer_scope &scope
)
{
	if (error)
	{
		std::rethrow_exception(error);
	}

	// The stretch is staged, so its runs are written from there.
	const auto range = static_cast<std::size_t>(tag);
	const auto step = m_range_steps[range];
	const auto first = m_plan.get_first_write(range);
	const auto count = m_plan.get_write_count(range);
	for (auto write = first; write < first + count; ++write)
	{
		scope.queue_write(
			m_file->get_file(),
			m_plan.get_offset(write),
			m_plan.get_buffers(write),
			write
		);
		++m_pending[step];
	}

	VITRIO_ASSERT(m_pending[step] > 0);
	--m_pending[step];
	send_ahead(scope);
}

void staged_image_write_operation::convert(std::size_t range) const
{
	const auto element_size = get_size(m_file_type);
	const auto origin = (m_range_offsets[range] - m_data_offset) / element_size;

	write_regions(
		m_walk,
		m_first_regions[range],
		m_region_counts[range],
		m_array_data,
		m_array_type,
		m_staged[range],
		static_cast<std::ptrdiff_t>(origin),
		m_file_type,
		m_file_order
	);
}

// Converts the step that is not done with and the one after it, past the
// steps whose conversions and writes are all done.
void staged_image_write_operation::send_ahead(file_transfer_scope &scope)
{
	const auto step_count = m_pending.size();
	for (;;)
	{
		while (m_sent_steps < step_count && m_sent_steps < m_done_steps + 2)
		{
			const auto step = m_sent_steps++;
			const auto first = m_step_first_ranges[step];
			const auto end = m_step_first_ranges[step + 1];
			for (auto range = first; range < end; ++range)
			{
				scope.run(std::make_unique<conversion>(*this, range), range);
				++m_pending[step];
			}
		}

		if (m_done_steps == step_count || m_pending[m_done_steps] > 0)
		{
			return;
		}

		++m_done_steps;
	}
}

} // namespace vitrio
