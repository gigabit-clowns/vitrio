// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @def VITRIO_STD_BASE_INTERFACE
 * @brief Silence MSVC warning C4275 for the class declared right after it.
 *
 * C4275 fires when an exported class (see VITRIO_API) derives from a
 * standard library type, such as std::runtime_error, that is not itself
 * exported. This is safe as long as every module links against the same
 * dynamic C++ runtime (/MD), so the base class has a single shared
 * definition.
 *
 * Placed on its own line immediately before the class declaration, it
 * suppresses the warning for that single line only, so it needs no closing
 * macro. The class head and its base clause must therefore be kept together
 * on that one line. Expands to nothing on other compilers.
 */
#if defined(_MSC_VER)
	#define VITRIO_STD_BASE_INTERFACE __pragma(warning(suppress: 4275))
#else
	#define VITRIO_STD_BASE_INTERFACE
#endif

/**
 * @def VITRIO_STD_MEMBER_INTERFACE
 * @brief Silence MSVC warning C4251 for the data member declared right after
 * it.
 *
 * C4251 fires when an exported class (see VITRIO_API) has a data member
 * whose type is not itself exported, typically a standard library type such
 * as the std::unique_ptr of the pimpl idiom or a std::string. This is safe as
 * long as every module links against the same dynamic C++ runtime (/MD), so
 * the type has a single shared definition, and the member is not manipulated
 * across the DLL boundary, which a private member never is.
 *
 * Placed on its own line immediately before a member declaration, it
 * suppresses the warning for that single line only, so it needs no closing
 * macro and leaves the warning active everywhere else. Expands to nothing on
 * other compilers.
 */
#if defined(_MSC_VER)
	#define VITRIO_STD_MEMBER_INTERFACE __pragma(warning(suppress: 4251))
#else
	#define VITRIO_STD_MEMBER_INTERFACE
#endif
