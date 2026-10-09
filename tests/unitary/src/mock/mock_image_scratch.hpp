// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_scratch.hpp>

#include <vitrio/image_scratch_entry.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_scratch final
	: public image_scratch
{
public:
	mock_image_scratch() = default;

	MAKE_MOCK1(
		find,
		std::shared_ptr<image_scratch_entry>(const std::string &path),
		override
	);
};

} // namespace vitrio
