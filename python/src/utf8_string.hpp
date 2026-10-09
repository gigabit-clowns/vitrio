// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <filesystem>
#include <string>

namespace vitrio
{

/**
 * @brief Get the text of a key or of a path given from Python.
 *
 * Python gives either as a string or as an object that names a path, and
 * nanobind takes both as a path.
 *
 * @param path The key or the path.
 * @return std::string Its text, encoded in UTF-8 and otherwise unchanged.
 */
std::string to_utf8_string(const std::filesystem::path &path);

} // namespace vitrio
