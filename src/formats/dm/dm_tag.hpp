// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vitrio
{
namespace dm
{

/**
 * @brief One entry of the tree of tags a DigitalMicrograph file holds: a
 * group of further entries, or a datum.
 *
 * Entries are labelled, though many are not: the entries of a list, such as
 * the images of a file, carry an empty label and are told apart by their
 * position. A datum does not hold its value, only how it is encoded and where
 * it lies in the file, so that the tree of a file is small whatever its
 * images are.
 *
 * An entry owns its children and can only be moved.
 */
class dm_tag
{
public:
	/**
	 * @brief Construct a group with no entries.
	 *
	 * @param label The label of the group.
	 */
	explicit dm_tag(std::string label);

	/**
	 * @brief Construct a datum.
	 *
	 * @param label The label of the datum.
	 * @param encoding The words that state how its value is encoded.
	 * @param payload_offset Where its value begins in the file, in bytes.
	 * @param payload_size The size of its value, in bytes.
	 */
	dm_tag(
		std::string label,
		std::vector<std::uint64_t> encoding,
		std::size_t payload_offset,
		std::size_t payload_size
	);

	dm_tag(const dm_tag &other) = delete;
	dm_tag(dm_tag &&other) noexcept;
	~dm_tag();

	dm_tag& operator=(const dm_tag &other) = delete;
	dm_tag& operator=(dm_tag &&other) noexcept;

	/**
	 * @brief Check whether the entry is a group.
	 *
	 * @return bool true for a group and false for a datum.
	 */
	bool is_group() const noexcept;

	/**
	 * @brief Get the label of the entry.
	 *
	 * @return const std::string& The label, which may be empty.
	 */
	const std::string& get_label() const noexcept;

	/**
	 * @brief Append an entry to a group.
	 *
	 * @param child The entry. Must be called on a group.
	 */
	void add_child(dm_tag child);

	/**
	 * @brief Get how many entries a group holds.
	 *
	 * @return std::size_t The number of entries, zero for a datum.
	 */
	std::size_t get_child_count() const noexcept;

	/**
	 * @brief Get one entry of a group.
	 *
	 * @param index Position of the entry. Must be below
	 * @ref get_child_count.
	 * @return const dm_tag& The entry.
	 */
	const dm_tag& get_child(std::size_t index) const noexcept;

	/**
	 * @brief Find the first entry of a group with a given label.
	 *
	 * @param label The label.
	 * @return const dm_tag* The entry, or nullptr if the group has none of
	 * that label or this is a datum.
	 */
	const dm_tag* find_child(const std::string &label) const noexcept;

	/**
	 * @brief Get how the value of a datum is encoded.
	 *
	 * @return span<const std::uint64_t> The words of its encoding, none for
	 * a group.
	 */
	span<const std::uint64_t> get_encoding() const noexcept;

	/**
	 * @brief Get where the value of a datum begins in the file.
	 *
	 * @return std::size_t The offset, in bytes. Zero for a group.
	 */
	std::size_t get_payload_offset() const noexcept;

	/**
	 * @brief Get the size of the value of a datum.
	 *
	 * @return std::size_t The size, in bytes. Zero for a group.
	 */
	std::size_t get_payload_size() const noexcept;

private:
	bool m_group;
	std::string m_label;
	std::vector<std::unique_ptr<dm_tag>> m_children;
	std::vector<std::uint64_t> m_encoding;
	std::size_t m_payload_offset;
	std::size_t m_payload_size;
};

} // namespace dm
} // namespace vitrio
