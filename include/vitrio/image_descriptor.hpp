// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <functional>
#include <vector>

namespace vitrio
{

/**
 * @brief The shape and data type of the values an image file holds.
 *
 * The extents alone do not tell a stack of @c N images from one volume of
 * @c N planes, since both are @c (N,H,W). The core rank does: two for the
 * stack, three for the volume.
 *
 * How the values are laid out within the extents is not part of it.
 *
 * @see get_core_extents
 */
class image_descriptor
{
public:
	/**
	 * @brief Construct a descriptor from its components.
	 *
	 * @param extents Extents of the file, slowest axis first.
	 * @param core_rank How many trailing extents are one image or volume.
	 * @param data_type Data type of the elements.
	 * @throws std::invalid_argument If @p core_rank is zero or exceeds the
	 * rank of @p extents, or if @p data_type is unknown.
	 */
	VITRIO_API
	image_descriptor(
		span<const std::size_t> extents,
		std::size_t core_rank,
		numerical_type data_type
	);

	VITRIO_API
	image_descriptor(const image_descriptor &other);
	VITRIO_API
	image_descriptor(image_descriptor &&other) noexcept;
	VITRIO_API
	~image_descriptor();

	VITRIO_API
	image_descriptor& operator=(const image_descriptor &other);
	VITRIO_API
	image_descriptor& operator=(image_descriptor &&other) noexcept;

	/**
	 * @brief Get the hash value for this descriptor.
	 *
	 * @return std::size_t The hash value.
	 */
	VITRIO_API
	std::size_t hash() const noexcept;

	/**
	 * @brief Get the extents of the file.
	 *
	 * @return span<const std::size_t> The extents, slowest axis first. It
	 * refers to storage owned by this descriptor.
	 */
	VITRIO_API
	span<const std::size_t> get_extents() const noexcept;

	/**
	 * @brief Get how many of the extents are one image or volume.
	 *
	 * @return std::size_t The rank of one image or volume. Never zero, and
	 * never above the rank of @ref get_extents.
	 */
	VITRIO_API
	std::size_t get_core_rank() const noexcept;

	/**
	 * @brief Get the data type of the elements.
	 *
	 * @return numerical_type The data type. Never unknown.
	 */
	VITRIO_API
	numerical_type get_data_type() const noexcept;

	friend bool operator==(
		const image_descriptor &lhs,
		const image_descriptor &rhs
	) noexcept
	{
		return
			lhs.m_core_rank == rhs.m_core_rank &&
			lhs.m_data_type == rhs.m_data_type &&
			lhs.m_extents == rhs.m_extents;
	}

	friend bool operator!=(
		const image_descriptor &lhs,
		const image_descriptor &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::vector<std::size_t> m_extents;
	std::size_t m_core_rank;
	numerical_type m_data_type;
};

/**
 * @brief Get the extents of one image or volume.
 *
 * The trailing @ref image_descriptor::get_core_rank extents, so the axes the
 * file stacks along are left out.
 *
 * @param descriptor The descriptor to take them from.
 * @return span<const std::size_t> The extents of one image or volume. It
 * refers to storage owned by @p descriptor.
 */
VITRIO_API
span<const std::size_t>
get_core_extents(const image_descriptor &descriptor) noexcept;

} // namespace vitrio

namespace std
{

template<>
struct hash<vitrio::image_descriptor>
{
	std::size_t operator()(
		const vitrio::image_descriptor &descriptor
	) const noexcept
	{
		return descriptor.hash();
	}
};

} // namespace std
