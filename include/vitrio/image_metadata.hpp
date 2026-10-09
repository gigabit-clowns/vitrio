// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

namespace vitrio
{

/**
 * @brief What a file states beyond its shape and data type, such as how its
 * samples map onto physical space.
 *
 * TODO It carries no fields yet, so every instance states nothing.
 */
class image_metadata
{
public:
	/**
	 * @brief Construct metadata stating nothing.
	 */
	VITRIO_API
	image_metadata() noexcept;

	VITRIO_API
	image_metadata(const image_metadata &other);
	VITRIO_API
	image_metadata(image_metadata &&other) noexcept;
	VITRIO_API
	~image_metadata();

	VITRIO_API
	image_metadata& operator=(const image_metadata &other);
	VITRIO_API
	image_metadata& operator=(image_metadata &&other) noexcept;
};

} // namespace vitrio
