// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_loader.hpp"

#include <vitrio/image_loader.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_loader(nb::module_ &m)
{
	nb::class_<image_loader>(m, "ImageLoader");
}

} // namespace vitrio
