// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/synchronous_image_writer.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>

#include <exception>
#include <stdexcept>

namespace vitrio
{

namespace
{

void require_notifier(const std::shared_ptr<completion_notifier> &done)
{
	if (!done)
	{
		throw std::invalid_argument(
			"synchronous_image_writer: The notifier must not be null."
		);
	}
}

} // anonymous namespace

synchronous_image_writer::synchronous_image_writer() noexcept = default;

synchronous_image_writer::~synchronous_image_writer() = default;

void synchronous_image_writer::write_async(
	const_array source,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
)
{
	require_notifier(done);

	std::exception_ptr error;
	try
	{
		write(const_array_ref(source), regions);
	}
	catch (...)
	{
		error = std::current_exception();
	}
	done->notify_done(error);
}

void synchronous_image_writer::flush_async(
	std::shared_ptr<completion_notifier> done
)
{
	require_notifier(done);

	std::exception_ptr error;
	try
	{
		flush();
	}
	catch (...)
	{
		error = std::current_exception();
	}
	done->notify_done(error);
}

} // namespace vitrio
