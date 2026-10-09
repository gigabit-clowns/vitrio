// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "mrc_constants.hpp"

namespace vitrio
{
namespace mrc
{

inline
bool is_volume_stack_space_group(std::int32_t space_group) noexcept
{
	return space_group >= first_volume_stack_space_group &&
		space_group <= last_volume_stack_space_group;
}

} // namespace mrc
} // namespace vitrio
