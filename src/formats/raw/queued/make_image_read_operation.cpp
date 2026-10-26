// SPDX-License-Identifier: LGPL-2.1-or-later

#include "make_image_read_operation.hpp"

#include "direct_image_read_operation.hpp"
#include "staged_image_read_operation.hpp"

#include <formats/raw/planning/image_read_strategy.hpp>
#include <formats/raw/planning/image_region_split.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>

#include <array/array_data.hpp>
#include <tuning.hpp>

#include <utility>

namespace vitrio
{

std::unique_ptr<file_transfer_operation> make_image_read_operation(
	const std::string &path,
	std::shared_ptr<const boost::interprocess::file_mapping> file,
	std::size_t file_size,
	const raw_image_layout &layout,
	array destination,
	const image_transfer_plan &regions,
	std::size_t byte_budget
)
{
	// Refuses an array that is not initialized.
	get_array_data(array_ref(destination));
	const auto &target = destination.get_descriptor();

	// A region whose parts lie farther apart than a read bridges, such as a
	// slice across the planes of a volume, is read a part at a time, rather
	// than with everything between its parts.
	const auto parts = split_scattered_regions(
		regions,
		layout.get_strides(),
		layout.get_descriptor().get_data_type(),
		get_prefetch_gap_cap(),
		byte_budget
	);

	image_region_read_walk walk(
		parts,
		layout.get_descriptor().get_extents(),
		layout.get_strides(),
		target.get_extents(),
		target.get_strides(),
		target.get_offset()
	);

	const auto nothing_to_read =
		walk.get_offsets().get_region_count() == 0 ||
		walk.get_layout().compute_element_count() == 0;
	if (
		nothing_to_read ||
		lie_as_they_land(walk, target.get_data_type(), layout)
	)
	{
		return std::make_unique<direct_image_read_operation>(
			path,
			std::move(file),
			layout,
			std::move(destination),
			walk
		);
	}

	const auto schedule = schedule_staged_read(
		parts,
		walk,
		layout,
		file_size,
		byte_budget
	);
	return std::make_unique<staged_image_read_operation>(
		path,
		std::move(file),
		layout,
		std::move(destination),
		std::move(walk),
		schedule
	);
}

} // namespace vitrio
