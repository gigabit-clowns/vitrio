// SPDX-License-Identifier: LGPL-2.1-or-later

#include "executor_image_saver.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/executor_image_saver.hpp>
#include <vitrio/image_saver.hpp>
#include <vitrio/image_writer_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_executor_image_saver(nb::module_ &m)
{
	nb::class_<executor_image_saver, image_saver>(m, "ExecutorImageSaver")
		.def(
			nb::init<
				std::shared_ptr<image_writer_provider>,
				std::shared_ptr<executor>
			>(),
			nb::arg("writers"), nb::arg("executor")
		);
}

} // namespace vitrio
