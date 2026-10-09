// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

namespace vitrio
{

/**
 * @brief A query-only view of whether one or more units of work have
 * finished.
 *
 * Carries no way to mark itself done: that belongs to the disjoint
 * @ref completion_notifier, which a caller holding this is never handed.
 */
class VITRIO_API completion
{
public:
	completion() = default;
	completion(const completion &other) = default;
	completion(completion &&other) = default;
	virtual ~completion() = default;

	completion& operator=(const completion &other) = default;
	completion& operator=(completion &&other) = default;

	/**
	 * @brief Block the calling thread until this is ready.
	 */
	virtual void wait() const = 0;

	/**
	 * @brief Whether the work behind this has finished.
	 *
	 * @return bool True once every unit of work this stands for has run,
	 * successfully or not.
	 */
	virtual bool is_ready() const noexcept = 0;

	/**
	 * @brief Wait, then rethrow the first exception recorded, if any.
	 *
	 * @throws Whatever the first unit of work to fail threw.
	 */
	virtual void get() const = 0;
};

} // namespace vitrio
