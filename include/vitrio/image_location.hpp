// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <limits>
#include <ostream>
#include <string>

namespace vitrio
{

/**
 * @brief Address of an image file, or of one image or volume of a stack.
 *
 * The pair of the key of a file and a zero based index along the axis the
 * file stacks along, its slowest. The index @ref no_stack_index addresses the
 * file as a whole instead, which is how a file holding a single image or
 * volume is named.
 *
 * The key is what a provider is asked for the file with. It is often the
 * path of the file, and it is held as a plain string.
 *
 * @see parse_image_location
 */
class image_location
{
public:
	/**
	 * @brief Index addressing the file as a whole rather than one image or
	 * volume of it.
	 */
	VITRIO_API
	static constexpr std::size_t no_stack_index =
		std::numeric_limits<std::size_t>::max();

	/**
	 * @brief Construct a location from its components.
	 *
	 * @param key Key of the file, such as its path.
	 * @param index_in_stack Zero based index of the image or volume along the
	 * slowest axis of the file, or @ref no_stack_index to address the whole
	 * file.
	 */
	VITRIO_API
	explicit image_location(
		std::string key,
		std::size_t index_in_stack = no_stack_index
	);

	/**
	 * @brief Construct a location with an empty key and no index.
	 */
	VITRIO_API
	image_location() noexcept;
	VITRIO_API
	image_location(const image_location &other);
	VITRIO_API
	image_location(image_location &&other) noexcept;
	VITRIO_API
	~image_location();

	VITRIO_API
	image_location& operator=(const image_location &other);
	VITRIO_API
	image_location& operator=(image_location &&other) noexcept;

	/**
	 * @brief Get the hash value for this location.
	 *
	 * @return std::size_t The hash value.
	 */
	VITRIO_API
	std::size_t hash() const noexcept;

	/**
	 * @brief Get the key of the file.
	 *
	 * @return const std::string& Reference to the stored key. The reference
	 * is valid for the lifetime of this @ref image_location and is
	 * invalidated by assignment to or destruction of the object.
	 */
	VITRIO_API
	const std::string& get_key() const noexcept;

	/**
	 * @brief Get the index of the image or volume within the stack.
	 *
	 * @return std::size_t The zero based index along the slowest axis of the
	 * file, or @ref no_stack_index when the location addresses the file
	 * as a whole.
	 */
	VITRIO_API
	std::size_t get_index_in_stack() const noexcept;

	/**
	 * @brief Check whether this addresses one image or volume of its file
	 * rather than the file as a whole.
	 *
	 * @return true It carries an index along the slowest axis of the file.
	 * @return false Its index in the stack is @ref no_stack_index.
	 */
	VITRIO_API
	bool has_index_in_stack() const noexcept;

	friend bool
	operator==(const image_location &lhs, const image_location &rhs) noexcept
	{
		return
			lhs.get_index_in_stack() == rhs.get_index_in_stack() &&
			lhs.get_key() == rhs.get_key();
	}

	friend bool
	operator!=(const image_location &lhs, const image_location &rhs) noexcept
	{
		return !(lhs == rhs);
	}

	friend bool
	operator<(const image_location &lhs, const image_location &rhs) noexcept
	{
		if (lhs.get_key() < rhs.get_key())
		{
			return true;
		}
		else if (lhs.get_key() == rhs.get_key())
		{
			return lhs.get_index_in_stack() < rhs.get_index_in_stack();
		}
		return false;
	}

	friend bool
	operator<=(const image_location &lhs, const image_location &rhs) noexcept
	{
		return !(rhs < lhs);
	}

	friend bool
	operator>(const image_location &lhs, const image_location &rhs) noexcept
	{
		return rhs < lhs;
	}

	friend bool
	operator>=(const image_location &lhs, const image_location &rhs) noexcept
	{
		return !(lhs < rhs);
	}

	friend std::ostream&
	operator<<(std::ostream &os, const image_location &location)
	{
		if (location.has_index_in_stack())
		{
			os << (location.get_index_in_stack() + 1) << '@';
		}
		return os << location.get_key();
	}

private:
	std::string m_key;
	std::size_t m_index_in_stack;
};

/**
 * @brief Parse an image location from its string representation.
 *
 * The representation is expected to be:
 * `<index>@<key>` (a one based index into the file)
 * `<key>` (addresses the file as a whole)
 *
 * The index is one based to match the convention used by the star files of
 * the field, while @ref image_location::get_index_in_stack is zero based;
 * this function bridges the two. An index of zero is therefore not a valid
 * representation.
 *
 * @param text Appropriately formatted string with the location.
 * @param result Output image_location object.
 * @return true The string was parsed successfully and the result was written.
 * @return false The string was not parsed and the result was not written.
 *
 * @see to_string
 */
VITRIO_API
bool parse_image_location(const std::string &text, image_location &result);

/**
 * @brief Write an image location as its string representation.
 *
 * The exact inverse of @ref parse_image_location: a location addressing the
 * whole file is written as a bare key, and any other location is prefixed
 * with its one based index.
 *
 * @param location The location to be written.
 * @return std::string The string representation.
 */
VITRIO_API
std::string to_string(const image_location &location);

} // namespace vitrio

namespace std
{

template<>
struct hash<vitrio::image_location>
{
	std::size_t operator()(
		const vitrio::image_location &location
	) const noexcept
	{
		return location.hash();
	}
};

} // namespace std
