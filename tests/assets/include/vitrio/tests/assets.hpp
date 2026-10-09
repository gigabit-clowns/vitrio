// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

#ifndef VITRIO_TEST_SCRATCH_ROOT
	#error "VITRIO_TEST_SCRATCH_ROOT is not defined. Link against " \
	       "vitrio-test-assets-interface to consume the shared test assets."
#endif

namespace vitrio
{

inline std::string get_scratch_root()
{
	return VITRIO_TEST_SCRATCH_ROOT;
}

/**
 * @brief Get a path a test may write to.
 *
 * Under the build tree rather than the working directory, so that a test
 * writes where it is meant to whatever it was run from.
 *
 * The file is not created, and removing it is the caller's to do.
 */
inline std::string get_scratch_path(const std::string &name)
{
	#if defined(_WIN32)
		return get_scratch_root() + "\\" + name;
	#else
		return get_scratch_root() + "/" + name;
	#endif
}

} // namespace vitrio
