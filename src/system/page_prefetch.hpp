// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{

/**
 * @brief Ask for stretches of mapped memory to be paged in.
 *
 * This is advice and nothing more. It never fails, it changes no byte, and a
 * stretch that was advised may still have to be faulted in when it is read.
 *
 * Advice is taken in whole pages, so a stretch is widened to the pages it
 * touches: one that starts inside a page is advised from the first byte of
 * that page. Every stretch must lie within memory that is mapped, and so do
 * the pages it touches.
 *
 * A whole batch is taken at once because the Windows call serves every
 * stretch in one call. The stretches of one batch need not belong to the
 * same mapping.
 *
 * @param ranges The stretches to advise.
 */
void prefetch_pages(span<const memory_range> ranges) noexcept;

} // namespace vitrio
