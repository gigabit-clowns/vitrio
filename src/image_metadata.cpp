// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/image_metadata.hpp>

namespace vitrio
{

image_metadata::image_metadata() noexcept = default;

image_metadata::image_metadata(const image_metadata &other) = default;
image_metadata::image_metadata(image_metadata &&other) noexcept = default;
image_metadata::~image_metadata() = default;

image_metadata&
image_metadata::operator=(const image_metadata &other) = default;
image_metadata&
image_metadata::operator=(image_metadata &&other) noexcept = default;

} // namespace vitrio
