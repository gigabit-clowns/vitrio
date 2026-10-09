// SPDX-License-Identifier: LGPL-2.1-or-later

#include "thread_pool_executor.hpp"

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <cstddef>
#include <memory>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

// Destroying a pool waits for its workers, which takes as long as the work
// they have left. The thread that destroys it holds the GIL, and lets go of
// it meanwhile.
std::shared_ptr<thread_pool_executor> make_pool(std::size_t worker_count)
{
	auto pool = std::make_unique<thread_pool_executor>(worker_count);
	return std::shared_ptr<thread_pool_executor>(
		pool.release(),
		[] (thread_pool_executor *released) noexcept
		{
			const nb::gil_scoped_release release;
			delete released;
		}
	);
}

} // anonymous namespace

void bind_thread_pool_executor(nb::module_ &m)
{
	nb::class_<thread_pool_executor, executor>(m, "ThreadPoolExecutor")
		.def(nb::new_(&make_pool), nb::arg("worker_count"))
		.def_prop_ro(
			"worker_count",
			&thread_pool_executor::get_worker_count
		);
}

} // namespace vitrio
