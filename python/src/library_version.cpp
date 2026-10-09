// SPDX-License-Identifier: LGPL-2.1-or-later

#include "library_version.hpp"

#include <vitrio/library_version.hpp>

#include <nanobind/stl/tuple.h>

#include <cstdint>
#include <tuple>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>
py_get_library_version()
{
	const auto version = get_library_version();
	return std::make_tuple(
		version.get_major(),
		version.get_minor(),
		version.get_patch()
	);
}

} // anonymous namespace

void bind_library_version(nb::module_ &m)
{
	m.def("get_library_version", &py_get_library_version);
}

} // namespace vitrio
