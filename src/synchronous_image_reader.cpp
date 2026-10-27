// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/synchronous_image_reader.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>

#include <exception>
#include <stdexcept>

namespace vitrio
{

synchronous_image_reader::synchronous_image_reader() noexcept = default;

synchronous_image_reader::~synchronous_image_reader() = default;

void synchronous_image_reader::read_async(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	if (!done)
	{
		throw std::invalid_argument(
			"synchronous_image_reader::read_async: The notifier must not be "
			"null."
		);
	}

	std::exception_ptr error;
	try
	{
		read(array_ref(destination), regions);
	}
	catch (...)
	{
		error = std::current_exception();
	}
	done->notify_done(error);
}

} // namespace vitrio
