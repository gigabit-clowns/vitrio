// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <string>

namespace vitrio
{

/**
 * @brief Create a file of a given size, replacing whatever is at that path.
 *
 * A file being written is laid out in full before anything is written to
 * it, so that a region can be written wherever it belongs, in any order, and
 * so that a mapping of it, which can neither create a file nor resize one,
 * covers it. The bytes it is laid out with are unspecified.
 *
 * @param path Path to the file to create.
 * @param size Size of the file in bytes. Must not be zero: a file of no
 * bytes holds no image, and can not be mapped.
 * @throws image_file_error If the file could not be created or sized.
 */
void create_image_file(const std::string &path, std::size_t size);

} // namespace vitrio
