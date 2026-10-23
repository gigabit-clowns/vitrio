// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_transfer_pool.hpp>

#include "file_transfer_pool_implementation.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

file_transfer_pool::file_transfer_pool(std::shared_ptr<executor> tasks)
{
	if (!tasks)
	{
		throw std::invalid_argument(
			"file_transfer_pool: The executor must not be null."
		);
	}

	m_implementation = std::make_unique<implementation>(std::move(tasks));
}

file_transfer_pool::~file_transfer_pool() = default;

const file_transfer_pool::implementation&
file_transfer_pool::get_implementation() const noexcept
{
	return *m_implementation;
}

} // namespace vitrio
