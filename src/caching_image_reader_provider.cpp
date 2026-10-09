// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/caching_image_reader_provider.hpp>

#include <vitrio/image_reader.hpp>

#include <assert.hpp>

#include <list>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace vitrio
{

class caching_image_reader_provider::implementation
{
public:
	implementation(
		std::shared_ptr<image_reader_provider> backing,
		std::size_t capacity
	)
		: m_backing(std::move(backing))
		, m_capacity(capacity)
	{
	}

	std::size_t get_capacity() const noexcept
	{
		return m_capacity;
	}

	std::size_t get_reader_count() const noexcept
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_entries.size();
	}

	std::shared_ptr<const image_reader> acquire(const std::string &key)
	{
		auto reader = touch(key);
		if (reader)
		{
			return reader;
		}

		reader = m_backing->acquire(key);
		VITRIO_ASSERT(reader);
		store(key, reader);
		return reader;
	}

private:
	class cached_reader
	{
	public:
		cached_reader(
			std::string key,
			std::shared_ptr<const image_reader> reader
		)
			: m_key(std::move(key))
			, m_reader(std::move(reader))
		{
		}

		const std::string& get_key() const noexcept
		{
			return m_key;
		}

		const std::shared_ptr<const image_reader>&
		get_reader() const noexcept
		{
			return m_reader;
		}

	private:
		std::string m_key;
		std::shared_ptr<const image_reader> m_reader;
	};

	using cached_reader_list = std::list<cached_reader>;

	mutable std::mutex m_mutex;
	std::shared_ptr<image_reader_provider> m_backing;
	std::size_t m_capacity;
	cached_reader_list m_entries;
	std::unordered_map<std::string, cached_reader_list::iterator> m_index;

	std::shared_ptr<const image_reader> touch(const std::string &key)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto ite = m_index.find(key);
		if (ite == m_index.end())
		{
			return nullptr;
		}

		m_entries.splice(m_entries.begin(), m_entries, ite->second);
		return ite->second->get_reader();
	}

	void store(
		const std::string &key,
		const std::shared_ptr<const image_reader> &reader
	)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto ite = m_index.find(key);
		if (ite != m_index.end())
		{
			*ite->second = cached_reader(key, reader);
			m_entries.splice(m_entries.begin(), m_entries, ite->second);
			return;
		}

		if (m_entries.size() >= m_capacity)
		{
			evict_oldest();
		}

		m_entries.emplace_front(key, reader);
		m_index.emplace(key, m_entries.begin());
	}

	void evict_oldest()
	{
		VITRIO_ASSERT(!m_entries.empty());
		const auto victim = std::prev(m_entries.end());
		m_index.erase(victim->get_key());
		m_entries.erase(victim);
	}
};

caching_image_reader_provider::caching_image_reader_provider(
	std::shared_ptr<image_reader_provider> backing,
	std::size_t capacity
)
{
	if (!backing)
	{
		throw std::invalid_argument(
			"caching_image_reader_provider: The backing provider must not "
			"be null."
		);
	}

	if (capacity == 0)
	{
		throw std::invalid_argument(
			"caching_image_reader_provider: The capacity must be greater "
			"than zero."
		);
	}

	m_implementation = std::make_unique<implementation>(
		std::move(backing),
		capacity
	);
}

caching_image_reader_provider::~caching_image_reader_provider() = default;

std::size_t caching_image_reader_provider::get_capacity() const noexcept
{
	return m_implementation->get_capacity();
}

std::size_t caching_image_reader_provider::get_reader_count() const noexcept
{
	return m_implementation->get_reader_count();
}

std::shared_ptr<const image_reader>
caching_image_reader_provider::acquire(const std::string &key)
{
	return m_implementation->acquire(key);
}

} // namespace vitrio
