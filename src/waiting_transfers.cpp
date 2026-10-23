// SPDX-License-Identifier: LGPL-2.1-or-later

#include "waiting_transfers.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_writer.hpp>

#include <memory>
#include <utility>

namespace vitrio
{

void read_and_wait(
	const image_reader &reader,
	array destination,
	image_transfer_plan regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	reader.read_async(std::move(destination), std::move(regions), done);
	done->get();
}

void write_and_wait(
	image_writer &writer,
	const_array source,
	image_transfer_plan regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	writer.write_async(std::move(source), std::move(regions), done);
	done->get();
}

void flush_and_wait(image_writer &writer)
{
	const auto done = std::make_shared<counting_completion>(1);
	writer.flush_async(done);
	done->get();
}

} // namespace vitrio
