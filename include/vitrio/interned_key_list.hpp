// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief A list of keys in which equal keys are held once.
 *
 * Each entry of the list refers to a key, and entries naming the same key
 * share one copy of it: the distinct keys are held once and the entries are
 * indices into them. A list naming a few keys many times over therefore
 * costs a string per distinct key rather than per entry.
 *
 * The list has two sizes. @ref get_key_count is how many distinct keys
 * were interned and @ref get_entry_count how many entries there are. Each is
 * addressed on its own: @ref get_key takes a key index, @ref get an entry
 * index.
 *
 * @ref clear keeps both capacities, so a list refilled after clearing
 * allocates only the strings of the keys it interns.
 */
class interned_key_list
{
public:
	/**
	 * @brief Index reported for a key that was never interned.
	 */
	VITRIO_API
	static constexpr std::size_t no_key =
		std::numeric_limits<std::size_t>::max();

	/**
	 * @brief Construct a list holding no key and no entry.
	 */
	VITRIO_API
	interned_key_list() noexcept;

	VITRIO_API
	interned_key_list(const interned_key_list &other);
	VITRIO_API
	interned_key_list(interned_key_list &&other) noexcept;
	VITRIO_API
	~interned_key_list();

	VITRIO_API
	interned_key_list& operator=(const interned_key_list &other);
	VITRIO_API
	interned_key_list& operator=(interned_key_list &&other) noexcept;

	/**
	 * @brief Intern a key without appending an entry.
	 *
	 * A key equal to one already interned yields the index it was given
	 * the first time rather than a second one, so a key can be interned
	 * once and referred to by index afterwards.
	 *
	 * The interned keys are searched one by one, so this takes time in
	 * proportion to the number of distinct keys, not of entries.
	 *
	 * @param key The key to intern.
	 * @return std::size_t Index of the key, below @ref get_key_count.
	 */
	VITRIO_API
	std::size_t intern(std::string key);

	/**
	 * @brief Append one entry referring to an interned key.
	 *
	 * @param key_index Index of the key, as @ref intern returned it. Must
	 * be below @ref get_key_count.
	 * @return std::size_t Position of the appended entry.
	 * @throws std::out_of_range If @p key_index names no interned key.
	 */
	VITRIO_API
	std::size_t append(std::size_t key_index);

	/**
	 * @brief Intern a key and append one entry referring to it.
	 *
	 * @param key The key to intern and refer to.
	 * @return std::size_t Position of the appended entry.
	 */
	VITRIO_API
	std::size_t append(std::string key);

	/**
	 * @brief Drop every key and every entry, keeping both capacities.
	 */
	VITRIO_API
	void clear() noexcept;

	/**
	 * @brief Make room without allocating later.
	 *
	 * @param keys Number of distinct keys to make room for.
	 * @param entries Number of entries to make room for.
	 */
	VITRIO_API
	void reserve(std::size_t keys, std::size_t entries);

	/**
	 * @brief Get the index a key was interned under.
	 *
	 * @param key The key to look up.
	 * @return std::size_t Its index, or @ref no_key when it was never
	 * interned.
	 */
	VITRIO_API
	std::size_t find(const std::string &key) const noexcept;

	/**
	 * @brief Get how many entries are held.
	 *
	 * @return std::size_t The number of entries.
	 */
	VITRIO_API
	std::size_t get_entry_count() const noexcept;

	/**
	 * @brief Get the key one entry refers to.
	 *
	 * @param entry_index Position of the entry. Must be below
	 * @ref get_entry_count.
	 * @return const std::string& The key. It refers to storage owned by
	 * this list, which interning into, assigning to or destroying it
	 * invalidates.
	 */
	VITRIO_API
	const std::string& get(std::size_t entry_index) const noexcept;

	/**
	 * @brief Get which interned key one entry refers to.
	 *
	 * @param entry_index Position of the entry. Must be below
	 * @ref get_entry_count.
	 * @return std::size_t The key index, below @ref get_key_count.
	 */
	VITRIO_API
	std::size_t get_key_index(std::size_t entry_index) const noexcept;

	/**
	 * @brief Get how many distinct keys are held.
	 *
	 * A key counts once however many entries refer to it, and also when
	 * none does, having been interned without an entry.
	 *
	 * @return std::size_t The number of distinct keys.
	 */
	VITRIO_API
	std::size_t get_key_count() const noexcept;

	/**
	 * @brief Get one of the interned keys.
	 *
	 * @param key_index Index of the key. Must be below
	 * @ref get_key_count.
	 * @return const std::string& The key. It refers to storage owned by
	 * this list, which interning into, assigning to or destroying it
	 * invalidates.
	 */
	VITRIO_API
	const std::string& get_key(std::size_t key_index) const noexcept;

private:
	std::vector<std::string> m_keys;
	std::vector<std::size_t> m_entries;
};

} // namespace vitrio
