// SPDX-License-Identifier: LGPL-2.1-or-later

#include "direct_image_write_operation.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/span.hpp>

#include <assert.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

// The end of the run of regions from `first` on that follow one another in
// the file.
std::size_t find_run_end(
	span<const std::ptrdiff_t> file_offsets,
	std::size_t first,
	std::size_t element_count
) noexcept
{
	const auto region_length = static_cast<std::ptrdiff_t>(element_count);

	auto last = first + 1;
	while (
		last < file_offsets.size() &&
		file_offsets[last] == file_offsets[last - 1] + region_length
	)
	{
		++last;
	}

	return last;
}

// The writes of the regions: straight from the array, the regions that
// follow one another in the file as one write, all in one group. Regions of
// no element are not written.
file_write_plan plan_writes(
	const raw_image_layout &layout,
	const image_region_write_walk &walk,
	const byte *array_data
)
{
	const auto element_size =
		get_size(layout.get_descriptor().get_data_type());
	const auto element_count = walk.get_layout().compute_element_count();
	const auto region_size = element_count * element_size;

	file_write_plan plan;
	plan.add_group();
	if (element_count == 0)
	{
		return plan;
	}

	const auto &offsets = walk.get_offsets();
	const auto file_offsets = offsets.get_file();
	const auto array_offsets = offsets.get_array();

	std::vector<const_memory_range> buffers;
	for (std::size_t first = 0; first < file_offsets.size();)
	{
		const auto last = find_run_end(file_offsets, first, element_count);

		buffers.clear();
		for (auto region = first; region < last; ++region)
		{
			const auto array_offset = array_offsets[region] *
				static_cast<std::ptrdiff_t>(element_size);
			buffers.emplace_back(array_data + array_offset, region_size);
		}

		const auto file_offset =
			static_cast<std::size_t>(file_offsets[first]) * element_size;
		plan.add_write(
			layout.get_data_offset() + file_offset,
			make_span(buffers)
		);

		first = last;
	}

	return plan;
}

} // anonymous namespace

direct_image_write_operation::direct_image_write_operation(
	std::shared_ptr<writable_image_file> file,
	const raw_image_layout &layout,
	const_array source,
	const image_region_write_walk &walk
)
	: m_file(std::move(file))
	, m_source(std::move(source))
	, m_plan(plan_writes(layout, walk, m_source.get_data()))
{
	VITRIO_ASSERT(m_file);
}

direct_image_write_operation::~direct_image_write_operation() = default;

void direct_image_write_operation::start(file_transfer_scope &scope)
{
	for (std::size_t write = 0; write < m_plan.get_write_count(); ++write)
	{
		scope.queue_write(
			m_file->get_file(),
			m_plan.get_offset(write),
			m_plan.get_buffers(write),
			write
		);
	}
}

void direct_image_write_operation::on_transfer(
	const file_transfer_completion &completion,
	file_transfer_scope &
)
{
	// The values of the stretch reached the file.
	take_write(m_file->get_path(), m_plan, completion);
}

void direct_image_write_operation::on_task(
	std::uint64_t,
	std::exception_ptr,
	file_transfer_scope &
)
{
	// It runs no task.
	VITRIO_ASSERT(false);
}

} // namespace vitrio
