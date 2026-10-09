// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{

class image_location;

/**
 * @brief Groups a list of image locations by file.
 *
 * Each file has one group, and the groups are in ascending order of key.
 * The grouping therefore does not depend on the order of the locations.
 *
 * A file is either addressed as a whole, or has the stack indices of its
 * locations. A file that any location addresses as a whole has no indices,
 * because the whole file includes them.
 *
 * Two locations belong to the same file if their keys are equal strings.
 */
class image_location_grouping
{
public:
	class group;

	/**
	 * @brief Group a list of locations by file.
	 *
	 * @param locations The locations to group.
	 */
	VITRIO_API
	explicit image_location_grouping(span<const image_location> locations);

	VITRIO_API
	image_location_grouping(const image_location_grouping &other);
	VITRIO_API
	image_location_grouping(image_location_grouping &&other) noexcept;
	VITRIO_API
	~image_location_grouping();

	VITRIO_API
	image_location_grouping&
	operator=(const image_location_grouping &other);
	VITRIO_API
	image_location_grouping&
	operator=(image_location_grouping &&other) noexcept;

	/**
	 * @brief Get the number of groups.
	 *
	 * @return std::size_t The number of groups, which is the number of
	 * files.
	 */
	VITRIO_API
	std::size_t get_group_count() const noexcept;

	/**
	 * @brief Get the group of a file.
	 *
	 * @param index Index of the group.
	 * @return group The group. It refers to storage owned by this grouping.
	 * @throws std::out_of_range If @p index is not below
	 * @ref get_group_count.
	 */
	VITRIO_API
	group get_group(std::size_t index) const;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::vector<std::string> m_keys;
	VITRIO_STD_MEMBER_INTERFACE
	std::vector<std::size_t> m_indices;
	VITRIO_STD_MEMBER_INTERFACE
	std::vector<std::size_t> m_first_positions;
};

/**
 * @brief The locations of one file in an @ref image_location_grouping.
 *
 * A group refers to storage owned by its grouping, so it can be used only
 * while the grouping is alive and has not been assigned to.
 */
class image_location_grouping::group
{
public:
	/**
	 * @brief Get the key of the file.
	 *
	 * @return const std::string& The key.
	 */
	VITRIO_API
	const std::string& get_key() const noexcept;

	/**
	 * @brief Check whether any location addresses the file as a whole.
	 *
	 * @return true At least one location of the file has no stack index.
	 * @return false All locations of the file have a stack index.
	 */
	VITRIO_API
	bool is_whole() const noexcept;

	/**
	 * @brief Get the stack indices of the locations of the file.
	 *
	 * @return span<const std::size_t> The indices, in ascending order and
	 * without repeats. Empty if the file is addressed as a whole.
	 */
	VITRIO_API
	span<const std::size_t> get_indices() const noexcept;

private:
	friend class image_location_grouping;

	group(const std::string &key, span<const std::size_t> indices) noexcept;

	const std::string *m_key;
	span<const std::size_t> m_indices;
};

} // namespace vitrio
