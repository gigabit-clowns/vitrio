// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_loader.hpp"

#include <vitrio/image_loader.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_image_loader(nb::module_ &m)
{
	nb::class_<image_loader>(
		m, "ImageLoader",
		"Reads batches of images and patches asynchronously.\n"
		"\n"
		"It is what `read_batch_async` and `read_patches_async` dispatch "
		"their reads to. `vitrio.loader` assembles one."
	);
}

} // namespace vitrio
