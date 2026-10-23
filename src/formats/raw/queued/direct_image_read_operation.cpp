// SPDX-License-Identifier: LGPL-2.1-or-later

#include "direct_image_read_operation.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/memory_range.hpp>
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

// The reads of the regions: straight into the array, the regions that follow
// one another in the file as one read, all in one step. Regions of no
// element are not read.
file_read_plan plan_reads(
	const raw_image_layout &layout,
	const image_region_read_walk &walk,
	byte *array_data
)
{
	const auto element_size =
		get_size(layout.get_descriptor().get_data_type());
	const auto element_count = walk.get_layout().compute_element_count();
	const auto region_size = element_count * element_size;

	file_read_plan plan;
	if (element_count == 0)
	{
		return plan;
	}

	const auto &offsets = walk.get_offsets();
	const auto file_offsets = offsets.get_file();
	const auto array_offsets = offsets.get_array();

	std::vector<memory_range> buffers;
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
		plan.add_read(
			0,
			layout.get_data_offset() + file_offset,
			make_span(buffers)
		);

		first = last;
	}

	return plan;
}

} // anonymous namespace

direct_image_read_operation::direct_image_read_operation(
	std::string path,
	std::shared_ptr<const boost::interprocess::file_mapping> file,
	const raw_image_layout &layout,
	array destination,
	const image_region_read_walk &walk
)
	: m_path(std::move(path))
	, m_file(std::move(file))
	, m_destination(std::move(destination))
	, m_plan(plan_reads(layout, walk, m_destination.get_data()))
	, m_execution(m_path, m_file->get_mapping_handle().handle, m_plan)
{
}

direct_image_read_operation::~direct_image_read_operation() = default;

void direct_image_read_operation::start(file_transfer_scope &scope)
{
	m_execution.start(scope);
}

void direct_image_read_operation::on_read(
	const file_read_completion &completion,
	file_transfer_scope &scope
)
{
	// The values of the stretch landed where they belong.
	m_execution.release(m_execution.take(completion), scope);
}

void direct_image_read_operation::on_task(
	std::uint64_t,
	std::exception_ptr,
	file_transfer_scope &
)
{
	// It runs no task.
	VITRIO_ASSERT(false);
}

} // namespace vitrio
