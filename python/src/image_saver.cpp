// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_saver.hpp"

#include <vitrio/image_saver.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_saver(nb::module_ &m)
{
	nb::class_<image_saver>(m, "ImageSaver");
}

} // namespace vitrio
