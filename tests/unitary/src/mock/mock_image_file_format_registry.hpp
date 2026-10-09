// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <trompeloeil.hpp>

namespace vitrio
{

template <typename Format>
class mock_image_file_format_registry final
{
public:
	using format_type = Format;

	mock_image_file_format_registry() = default;

	MAKE_MOCK1(add, void(std::unique_ptr<Format> (*factory)()));
};

} // namespace vitrio
