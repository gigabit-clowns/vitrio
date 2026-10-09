// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_reader_provider.hpp>

#include <vitrio/image_reader.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_image_reader_provider final
	: public image_reader_provider
{
public:
	mock_image_reader_provider() = default;

	MAKE_MOCK1(
		acquire,
		std::shared_ptr<const image_reader>(const std::string &key),
		override
	);
};

} // namespace vitrio
