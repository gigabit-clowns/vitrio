// SPDX-License-Identifier: LGPL-2.1-or-later

#include "utf8_string.hpp"

namespace vitrio
{

std::string to_utf8_string(const std::filesystem::path &path)
{
	// The type of the result depends on the C++ standard.
	const auto text = path.u8string();
	return std::string(text.begin(), text.end());
}

} // namespace vitrio
