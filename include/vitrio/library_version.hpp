// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/version.hpp>

namespace vitrio
{

/**
 * @brief Get the version of the vitrio library in use.
 *
 * This is the version of the shared library the program runs with. It may
 * differ from the version of the headers the program was compiled with.
 *
 * @return version The version of the library.
 */
VITRIO_API
version get_library_version() noexcept;

} // namespace vitrio
