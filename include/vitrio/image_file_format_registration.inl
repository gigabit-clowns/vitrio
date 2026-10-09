// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_file_format_registration.hpp"

namespace vitrio
{

template <typename Format, typename Registry>
inline image_file_format_registration<Format, Registry>
::image_file_format_registration(Registry &registry)
{
	registry.add(&create_format);
}

template <typename Format, typename Registry>
inline
std::unique_ptr<typename Registry::format_type>
image_file_format_registration<Format, Registry>::create_format()
{
	return std::make_unique<Format>();
}

} // namespace vitrio
