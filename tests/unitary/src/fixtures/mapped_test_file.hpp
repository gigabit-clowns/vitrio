// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/memory_mapping/image_file_access.hpp>
#include <formats/memory_mapping/image_file_mapping.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <system/file_handle.hpp>

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief A file of known bytes, mapped for reading, which is removed when a
 * case leaves it.
 *
 * Its byte at offset n holds n modulo 251, a prime, so that no two pages
 * of it hold the same bytes.
 */
class mapped_test_file
{
public:
	mapped_test_file(const std::string &name, std::size_t size)
		: m_path(name)
		, m_mapping(write(m_path.get(), size), image_file_access::read_only)
	{
	}

	const std::string& get_path() const noexcept
	{
		return m_path.get();
	}

	file_handle_t get_file() const noexcept
	{
		return m_mapping.get_file();
	}

	span<const byte> get_bytes() const noexcept
	{
		return make_span(
			static_cast<const byte*>(m_mapping.get_data()),
			m_mapping.get_size()
		);
	}

	// Whether every byte still holds what it was written with.
	bool holds_its_values() const noexcept
	{
		const auto bytes = get_bytes();
		for (std::size_t offset = 0; offset < bytes.size(); ++offset)
		{
			if (bytes[offset] != value_at(offset))
			{
				return false;
			}
		}

		return true;
	}

private:
	static byte value_at(std::size_t offset) noexcept
	{
		return static_cast<byte>(offset % 251);
	}

	static const std::string& write(const std::string &path, std::size_t size)
	{
		std::vector<char> values(size);
		for (std::size_t offset = 0; offset < size; ++offset)
		{
			values[offset] = static_cast<char>(value_at(offset));
		}

		std::ofstream file(path, std::ios::out | std::ios::binary);
		file.write(values.data(), static_cast<std::streamsize>(size));
		return path;
	}

	scoped_path m_path;
	image_file_mapping m_mapping;
};

} // namespace vitrio
