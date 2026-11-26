// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_metadata_value.hpp>
#include <vitrio/span.hpp>

#include <string>
#include <utility>
#include <vector>

namespace vitrio
{

/**
 * @brief What an image file states beyond its shape and data type, such as
 * the distance between its samples.
 *
 * A dictionary of values, each under a label. Nothing has to be stated:
 * what a file does not carry, or a format does not read, is absent.
 *
 * @par Labels
 * A label with no dot in it names a standard field, one of those of
 * @ref image_metadata_fields, which means the same whatever the format of
 * the file. Any other label has a prefix that ends at its first dot and
 * says whose it is, such as the format that defines it: @c mrc.labels.
 *
 * @par Standard fields
 * A standard field has one type, and the dictionary holds it as no other.
 *
 * @see image_metadata_field
 */
class image_metadata
{
public:
	/// A label and the value under it.
	using entry_type = std::pair<std::string, image_metadata_value>;

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

	/**
	 * @brief State a value under a label.
	 *
	 * Replaces the value already under that label, if any.
	 *
	 * An integer given for a standard field that is a real is held as the
	 * real of the same value.
	 *
	 * @param label The label.
	 * @param value The value.
	 * @throws std::invalid_argument If @p label is empty, if it has no dot
	 * and names no standard field, or if it names a standard field and
	 * @p value is neither of its type nor an integer given for a real. The
	 * metadata is left as it was.
	 */
	VITRIO_API
	void set(std::string label, image_metadata_value value);

	/**
	 * @brief Find the value under a label.
	 *
	 * @param label The label.
	 * @return const image_metadata_value* The value, or nullptr when nothing
	 * is stated under @p label. It refers to storage owned by this metadata,
	 * and is valid until the metadata is next modified.
	 */
	VITRIO_API
	const image_metadata_value*
	find(const std::string &label) const noexcept;

	/**
	 * @brief Get the value under a label.
	 *
	 * @param label The label.
	 * @return const image_metadata_value& The value. It refers to storage
	 * owned by this metadata, and is valid until the metadata is next
	 * modified.
	 * @throws std::out_of_range If nothing is stated under @p label.
	 */
	VITRIO_API
	const image_metadata_value& get(const std::string &label) const;

	/**
	 * @brief Stop stating anything under a label.
	 *
	 * @param label The label.
	 * @return true A value was under @p label and is gone.
	 * @return false Nothing was stated under @p label.
	 */
	VITRIO_API
	bool erase(const std::string &label) noexcept;

	/**
	 * @brief Get everything that is stated.
	 *
	 * @return span<const entry_type> The entries, sorted by label. It refers
	 * to storage owned by this metadata, and is valid until the metadata is
	 * next modified.
	 */
	VITRIO_API
	span<const entry_type> get_entries() const noexcept;

	friend bool operator==(
		const image_metadata &lhs,
		const image_metadata &rhs
	) noexcept
	{
		return lhs.m_entries == rhs.m_entries;
	}

	friend bool operator!=(
		const image_metadata &lhs,
		const image_metadata &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::vector<entry_type> m_entries;
};

} // namespace vitrio
