// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_region_copy.hpp"

#include "image_region_read_walk.hpp"
#include "image_region_transfer.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>

#include <cstddef>

namespace vitrio
{

void copy_regions(
	const_array_ref source,
	array_ref destination,
	const image_transfer_plan &regions
)
{
	const auto *source_storage = get_array_data(source);
	auto *destination_data = get_array_data(destination);
	if (regions.get_region_count() == 0)
	{
		return;
	}

	const auto &source_descriptor = source.get_descriptor();
	const auto &destination_descriptor = destination.get_descriptor();
	const image_region_read_walk walk(
		regions,
		source_descriptor.get_extents(),
		source_descriptor.get_strides(),
		destination_descriptor.get_extents(),
		destination_descriptor.get_strides(),
		destination_descriptor.get_offset()
	);

	const auto source_type = source_descriptor.get_data_type();
	const auto element_size =
		static_cast<std::ptrdiff_t>(get_size(source_type));
	const auto *source_data =
		source_storage + source_descriptor.get_offset() * element_size;

	read_regions(
		walk,
		destination_data,
		destination_descriptor.get_data_type(),
		source_data,
		source_type,
		get_system_byte_order()
	);
}

} // namespace vitrio
