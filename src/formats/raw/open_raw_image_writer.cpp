// SPDX-License-Identifier: LGPL-2.1-or-later

#include "open_raw_image_writer.hpp"

#include <formats/raw/queued/queued_image_writer.hpp>

#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_writer.hpp>

#include <file_transfer_pool_implementation.hpp>

#include <memory>
#include <utility>

namespace vitrio
{

std::shared_ptr<image_writer> open_raw_image_writer(
	const std::string &path,
	raw_image_layout layout,
	span<const byte> preamble,
	const file_transfer_pool &transfers
)
{
	return std::make_shared<queued_image_writer>(
		path,
		std::move(layout),
		preamble,
		transfers.get_implementation().get_executor()
	);
}

} // namespace vitrio
