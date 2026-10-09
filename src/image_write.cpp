// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_write.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/completion.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_saver.hpp>
#include <vitrio/image_transaction_plan.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_writer.hpp>
#include <vitrio/span.hpp>
#include <vitrio/strict_image_transfer_sanitizer.hpp>

#include <image_plan_builders.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

numerical_type resolve_data_type(
	const const_array_ref &arr,
	numerical_type data_type
) noexcept
{
	return data_type == numerical_type::unknown
		? arr.get_descriptor().get_data_type()
		: data_type;
}

} // anonymous namespace

void write_single(
	const_array_ref arr,
	const std::string &path,
	const image_file_write_format_selector &formats,
	numerical_type data_type,
	const image_metadata &metadata
)
{
	const auto extents = arr.get_descriptor().get_extents();
	if (extents.empty())
	{
		throw std::invalid_argument("write_single: The array has no extents.");
	}

	const image_descriptor descriptor(
		extents,
		extents.size(),
		resolve_data_type(arr, data_type)
	);

	write(arr, path, formats, descriptor, metadata);
}

void write_stack(
	const_array_ref arr,
	const std::string &path,
	const image_file_write_format_selector &formats,
	numerical_type data_type,
	const image_metadata &metadata
)
{
	const auto extents = arr.get_descriptor().get_extents();
	if (extents.size() < 2)
	{
		throw std::invalid_argument(
			"write_stack: The array needs a leading extent to stack along "
			"and at least one more for each image or volume."
		);
	}

	const image_descriptor descriptor(
		extents,
		extents.size() - 1,
		resolve_data_type(arr, data_type)
	);

	write(arr, path, formats, descriptor, metadata);
}

void write(
	const_array_ref arr,
	const std::string &path,
	const image_file_write_format_selector &formats,
	const image_descriptor &descriptor,
	const image_metadata &metadata
)
{
	const auto extents = arr.get_descriptor().get_extents();
	const auto file_extents = descriptor.get_extents();
	if (!std::equal(
			extents.begin(),
			extents.end(),
			file_extents.begin(),
			file_extents.end()
		))
	{
		throw std::invalid_argument(
			"write: The extents of the array are not those of the "
			"descriptor."
		);
	}

	const auto writer = formats.open(path, descriptor, metadata);
	writer->write(arr, make_location_plan(descriptor, image_location(path)));
	writer->flush();
}

std::shared_ptr<completion> write_batch_async(
	const image_saver &saver,
	const_array source,
	span<const image_location> locations
)
{
	const auto transaction = make_batch_plan(
		source.get_descriptor().get_extents(),
		locations
	);

	return saver.save(
		std::move(source),
		transaction,
		strict_image_transfer_sanitizer::get_shared()
	);
}

} // namespace vitrio
