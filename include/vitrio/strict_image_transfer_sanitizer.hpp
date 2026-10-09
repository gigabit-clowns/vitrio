// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_transfer_sanitizer.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief A sanitizer that refuses every region that does not fit.
 *
 * A plan whose regions all fit both sides is answered unchanged. A region
 * reaching past the file or past the array is refused with
 * @c std::out_of_range rather than shortened or dropped, so nothing of a
 * plan is transferred unless all of it can be.
 */
class VITRIO_API strict_image_transfer_sanitizer final
	: public image_transfer_sanitizer
{
public:
	std::vector<image_transfer_plan> sanitize(
		const image_transfer_plan &regions,
		span<const std::size_t> file_extents,
		span<const std::size_t> array_extents
	) const override;

	/**
	 * @brief Get the instance every use shares.
	 *
	 * @return const std::shared_ptr<const strict_image_transfer_sanitizer>&
	 * The instance, never null.
	 */
	static const std::shared_ptr<const strict_image_transfer_sanitizer>&
	get_shared();
};

} // namespace vitrio
