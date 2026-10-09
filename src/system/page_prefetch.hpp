// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "memory_range.hpp"

#include <vitrio/span.hpp>

namespace vitrio
{

/**
 * @brief Ask for stretches of mapped memory to be paged in.
 *
 * This is advice and nothing more. It never fails, and a stretch that was
 * advised may still have to be faulted in when it is read.
 *
 * Every stretch must start at the first byte of a page and lie within memory
 * that is mapped. Advice is taken in whole pages, so a stretch that started
 * inside a page would reach the bytes before it anyway. @ref get_page_size
 * is what the boundary is measured in.
 *
 * A whole batch is taken at once because the Windows call serves every
 * stretch in one call. The stretches of one batch need not belong to the
 * same mapping.
 *
 * @param ranges The stretches to advise.
 */
void prefetch_pages(span<const memory_range> ranges) noexcept;

} // namespace vitrio
