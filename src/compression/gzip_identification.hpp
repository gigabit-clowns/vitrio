// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief Tell whether bytes begin as a gzip stream does: with its magic
 * number and the method of deflate, `1f 8b 08`.
 *
 * @param bytes The leading bytes of a file.
 * @return bool Whether they do.
 */
bool has_gzip_signature(span<const byte> bytes) noexcept;

/**
 * @brief Get the extension a path has under `.gz`: that of the file it
 * holds compressed.
 *
 * @param path The path, such as `emd_3197.map.gz`.
 * @return std::string The extension under `.gz`, in lower case and with its
 * dot, such as `.map`. Empty where the path does not end in `.gz`, in any
 * case, or where its name has no other extension.
 */
std::string get_gzip_inner_extension(const std::string &path);

/**
 * @brief Inflate what the leading bytes of a gzip file hold, as far as they
 * go.
 *
 * The leading bytes are a stretch of the stream cut short, which is not an
 * error here: what they inflate into is returned, up to where they end or
 * stop being a stream zlib inflates.
 *
 * @param bytes The leading bytes of the file.
 * @param limit Most bytes to inflate.
 * @return std::vector<byte> What they inflate into, at most @p limit bytes.
 * Empty where they do not begin as a gzip stream does.
 */
std::vector<byte>
inflate_leading_bytes(span<const byte> bytes, std::size_t limit);

} // namespace vitrio
