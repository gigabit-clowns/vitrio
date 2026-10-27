// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief Tell the thread sanitizer that memory is about to be unmapped with
 * a system call it does not see.
 *
 * liburing maps the rings of an io_uring, and unmaps them, with system calls
 * of its own, which the thread sanitizer does not intercept. The kernel may
 * map a new ring where the ring of another thread was, and the sanitizer,
 * having never seen that one unmapped, takes the accesses to the new ring for
 * races with those to the old one. Each unmapping is announced as a release,
 * and each mapping as an acquire, of one token, which orders every unmapping
 * before the mappings that follow it.
 *
 * Outside the thread sanitizer it does nothing.
 */
void announce_unseen_unmapping() noexcept;

/**
 * @brief Tell the thread sanitizer that memory was just mapped with a system
 * call it does not see.
 *
 * See @ref announce_unseen_unmapping.
 */
void announce_unseen_mapping() noexcept;

} // namespace vitrio
