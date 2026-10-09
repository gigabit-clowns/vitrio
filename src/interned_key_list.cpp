// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/interned_key_list.hpp>

#include <assert.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vitrio
{

// A constant of a class needs a definition in C++14 to be bound to a
// reference. From C++17 on its declaration is one.
constexpr std::size_t interned_key_list::no_key;

interned_key_list::interned_key_list() noexcept = default;

interned_key_list::interned_key_list(
	const interned_key_list &other
) = default;
interned_key_list::interned_key_list(
	interned_key_list &&other
) noexcept = default;
interned_key_list::~interned_key_list() = default;

interned_key_list&
interned_key_list::operator=(const interned_key_list &other) = default;
interned_key_list&
interned_key_list::operator=(interned_key_list &&other) noexcept = default;

std::size_t interned_key_list::intern(std::string key)
{
	const auto found = find(key);
	if (found != no_key)
	{
		return found;
	}

	m_keys.push_back(std::move(key));
	return m_keys.size() - 1;
}

std::size_t interned_key_list::append(std::size_t key_index)
{
	if (key_index >= m_keys.size())
	{
		throw std::out_of_range(
			"interned_key_list::append: The key index names no interned "
			"key."
		);
	}

	m_entries.push_back(key_index);
	return m_entries.size() - 1;
}

std::size_t interned_key_list::append(std::string key)
{
	return append(intern(std::move(key)));
}

void interned_key_list::clear() noexcept
{
	m_keys.clear();
	m_entries.clear();
}

void interned_key_list::reserve(std::size_t keys, std::size_t entries)
{
	m_keys.reserve(keys);
	m_entries.reserve(entries);
}

std::size_t interned_key_list::find(const std::string &key) const noexcept
{
	const auto ite = std::find(m_keys.begin(), m_keys.end(), key);
	if (ite == m_keys.end())
	{
		return no_key;
	}

	return static_cast<std::size_t>(std::distance(m_keys.begin(), ite));
}

std::size_t interned_key_list::get_entry_count() const noexcept
{
	return m_entries.size();
}

const std::string&
interned_key_list::get(std::size_t entry_index) const noexcept
{
	return get_key(get_key_index(entry_index));
}

std::size_t
interned_key_list::get_key_index(std::size_t entry_index) const noexcept
{
	VITRIO_ASSERT(entry_index < m_entries.size());
	return m_entries[entry_index];
}

std::size_t interned_key_list::get_key_count() const noexcept
{
	return m_keys.size();
}

const std::string&
interned_key_list::get_key(std::size_t key_index) const noexcept
{
	VITRIO_ASSERT(key_index < m_keys.size());
	return m_keys[key_index];
}

} // namespace vitrio
