// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_scratch.hpp"

#include <vitrio/image_scratch.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_scratch(nb::module_ &m)
{
	nb::class_<image_scratch>(m, "ImageScratch");
}

} // namespace vitrio
