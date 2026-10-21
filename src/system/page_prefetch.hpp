// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{

/**
 * @brief Ask for stretches of a file mapped into memory to be read in.
 *
 * This is advice and nothing more. It never fails, it changes no byte, and a
 * stretch that was advised may still have to be faulted in when it is read:
 * the system may read in less than it is asked for. Nothing the advice
 * starts refers to the mapping once the call returns.
 *
 * Advice is taken in whole pages, so a stretch is widened to the pages it
 * touches: one that starts inside a page is advised from the first byte of
 * that page. Every stretch must lie within @p mapping, and so do the pages
 * it touches.
 *
 * A whole batch is taken at once because the Windows call serves every
 * stretch in one call.
 *
 * @param file The file the mapping is of.
 * @param mapping The bytes the file is mapped at, which hold it from its
 * first byte.
 * @param ranges The stretches to advise.
 */
void prefetch_pages(
	file_handle_t file,
	span<const byte> mapping,
	span<const memory_range> ranges
) noexcept;

} // namespace vitrio
