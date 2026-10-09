// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief Decides what becomes of the regions of a plan that do not fit the
 * file or the array they join.
 *
 * A plan is stated before the extents of its file are known, so a region of
 * it may reach past the file, past the array, or past both. A sanitizer is
 * shown the plan beside the extents of both sides and answers with the plans
 * to transfer in its place, every region of which fits both sides. What one
 * does with a region that does not fit, whether it shortens it, drops it or
 * refuses it, is what tells one sanitizer from another.
 *
 * @par Thread safety
 * @ref sanitize may be called concurrently.
 */
class VITRIO_API image_transfer_sanitizer
{
public:
	image_transfer_sanitizer() noexcept;
	image_transfer_sanitizer(const image_transfer_sanitizer &other) = delete;
	image_transfer_sanitizer(image_transfer_sanitizer &&other) = delete;
	virtual ~image_transfer_sanitizer();

	image_transfer_sanitizer&
	operator=(const image_transfer_sanitizer &other) = delete;
	image_transfer_sanitizer&
	operator=(image_transfer_sanitizer &&other) = delete;

	/**
	 * @brief Get the plans to transfer in place of a plan.
	 *
	 * Every region of every plan returned fits both sides. The plans have
	 * the file rank and the array rank of @p regions. Their extents may
	 * differ from those of @p regions and from one another, which is why
	 * more than one may be returned.
	 *
	 * @param regions The regions asked for.
	 * @param file_extents Extents of the file. Must have the file rank of
	 * @p regions.
	 * @param array_extents Extents of the array. Must have the array rank of
	 * @p regions.
	 * @return std::vector<image_transfer_plan> The plans to transfer. Empty
	 * when nothing is to be transferred.
	 * @throws std::invalid_argument If either extents do not have the rank
	 * of the side they describe.
	 * @throws std::out_of_range If a region that does not fit is refused.
	 */
	virtual std::vector<image_transfer_plan> sanitize(
		const image_transfer_plan &regions,
		span<const std::size_t> file_extents,
		span<const std::size_t> array_extents
	) const = 0;
};

} // namespace vitrio
