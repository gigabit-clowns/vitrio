// SPDX-License-Identifier: LGPL-2.1-or-later

#include "borrowed_array.hpp"

#include "array_memory.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/span.hpp>

#include <memory>

namespace vitrio
{

namespace
{

// What a borrowed array points its owner at. An owner must not be null, and
// the owner of a borrowed array owns nothing, so it points here.
char borrowed_memory_anchor;

std::shared_ptr<void> make_owner_of_nothing() noexcept
{
	return std::shared_ptr<void>(
		std::shared_ptr<void>(),
		&borrowed_memory_anchor
	);
}

} // anonymous namespace

array borrow_array(array_ref reference)
{
	const auto &descriptor = reference.get_descriptor();
	if (!is_initialized(descriptor))
	{
		return array();
	}

	return array(
		span<byte>(
			reference.get_data(),
			compute_storage_requirement(descriptor)
		),
		make_owner_of_nothing(),
		descriptor
	);
}

const_array borrow_array(const_array_ref reference)
{
	const auto &descriptor = reference.get_descriptor();
	if (!is_initialized(descriptor))
	{
		return const_array();
	}

	return const_array(
		span<const byte>(
			reference.get_data(),
			compute_storage_requirement(descriptor)
		),
		make_owner_of_nothing(),
		descriptor
	);
}

} // namespace vitrio
