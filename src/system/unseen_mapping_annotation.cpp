// SPDX-License-Identifier: LGPL-2.1-or-later

#include "unseen_mapping_annotation.hpp"

// GCC says it builds for the thread sanitizer with a macro, and Clang with a
// feature.
#if defined(__SANITIZE_THREAD__)
	#define VITRIO_THREAD_SANITIZER
#elif defined(__has_feature)
	#if __has_feature(thread_sanitizer)
		#define VITRIO_THREAD_SANITIZER
	#endif
#endif

#if defined(VITRIO_THREAD_SANITIZER)
	#include <sanitizer/tsan_interface.h>
#endif

namespace vitrio
{

#if defined(VITRIO_THREAD_SANITIZER)

namespace
{

// What every unmapping releases and every mapping acquires.
char mapping_token;

} // anonymous namespace

void announce_unseen_unmapping() noexcept
{
	__tsan_release(&mapping_token);
}

void announce_unseen_mapping() noexcept
{
	__tsan_acquire(&mapping_token);
}

#else

void announce_unseen_unmapping() noexcept
{
}

void announce_unseen_mapping() noexcept
{
}

#endif

} // namespace vitrio
