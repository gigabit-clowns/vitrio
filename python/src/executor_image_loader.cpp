// SPDX-License-Identifier: LGPL-2.1-or-later

#include "executor_image_loader.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/executor_image_loader.hpp>
#include <vitrio/image_loader.hpp>
#include <vitrio/image_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_executor_image_loader(nb::module_ &m)
{
	nb::class_<executor_image_loader, image_loader>(m, "ExecutorImageLoader")
		.def(
			nb::init<
				std::shared_ptr<image_reader_provider>,
				std::shared_ptr<executor>
			>(),
			nb::arg("readers"), nb::arg("executor")
		);
}

} // namespace vitrio
