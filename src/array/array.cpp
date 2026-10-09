// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/array.hpp>

#include "array_implementation.hpp"
#include "array_memory.hpp"

#include <memory/aligned_alloc.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

constexpr std::size_t allocation_alignment = 64;

std::size_t compute_allocation_size(const array_descriptor &descriptor)
{
	// An array with no elements still needs an owner, and an owner needs
	// something to own.
	const auto requirement =
		std::max<std::size_t>(compute_storage_requirement(descriptor), 1);

	const auto block_count = (requirement - 1) / allocation_alignment + 1;
	const auto highest = std::numeric_limits<std::size_t>::max();
	if (block_count > highest / allocation_alignment)
	{
		throw std::bad_alloc();
	}

	return block_count * allocation_alignment;
}

} // anonymous namespace

array::array() noexcept = default;

array::array(
	span<byte> memory,
	std::shared_ptr<void> owner,
	array_descriptor descriptor
)
{
	if (!owner)
	{
		throw std::invalid_argument("array: The owner must not be null.");
	}

	m_implementation = std::make_shared<array_implementation>(
		memory,
		std::move(owner),
		std::move(descriptor)
	);
}

array::array(
	std::shared_ptr<const array_implementation> implementation
) noexcept
	: m_implementation(std::move(implementation))
{
}

array::array(array &&other) noexcept = default;

array::~array() = default;

array& array::operator=(array &&other) noexcept = default;

const array_descriptor& array::get_descriptor() const noexcept
{
	return m_implementation
		? m_implementation->get_descriptor()
		: array_implementation::get_empty_descriptor();
}

byte* array::get_data() noexcept
{
	// The implementation holds the memory as read-only for every class
	// alike. This one was constructed over writable memory.
	return const_cast<byte*>(
		static_cast<const array&>(*this).get_data()
	);
}

const byte* array::get_data() const noexcept
{
	return m_implementation ? m_implementation->get_data() : nullptr;
}

array array::share() noexcept
{
	return array(m_implementation);
}

const_array array::share_const() const noexcept
{
	return const_array(m_implementation);
}

array make_array(const array_descriptor &descriptor)
{
	if (!is_initialized(descriptor))
	{
		throw std::invalid_argument(
			"make_array: The descriptor is not initialized."
		);
	}

	const auto size = compute_allocation_size(descriptor);
	auto *data = static_cast<byte*>(
		vitrio::aligned_alloc(size, allocation_alignment)
	);
	if (data == nullptr)
	{
		throw std::bad_alloc();
	}

	// Should the shared pointer fail to be constructed, it releases the
	// memory itself.
	std::shared_ptr<void> owner(data, &aligned_free);

	return array(make_span(data, size), std::move(owner), descriptor);
}

} // namespace vitrio
