// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_writer_provider.hpp>

#include <vitrio/image_writer.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_writer_provider final
	: public image_writer_provider
{
public:
	mock_image_writer_provider() = default;

	MAKE_MOCK1(
		acquire,
		std::shared_ptr<image_writer>(const std::string &path),
		override
	);
};

} // namespace vitrio
