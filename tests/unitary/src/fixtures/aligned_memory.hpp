// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>

#include <memory/aligned_alloc.hpp>

#include <cstddef>

namespace vitrio
{

/**
 * @brief A block of aligned memory that is freed when a case leaves it.
 *
 * It stands for a mapping, which starts on a page as this block starts on
 * its alignment.
 */
class aligned_memory
{
public:
	aligned_memory(std::size_t size, std::size_t alignment)
		: m_data(static_cast<byte*>(aligned_alloc(size, alignment)))
	{
	}

	aligned_memory(const aligned_memory &other) = delete;
	aligned_memory(aligned_memory &&other) = delete;

	~aligned_memory()
	{
		aligned_free(m_data);
	}

	aligned_memory& operator=(const aligned_memory &other) = delete;
	aligned_memory& operator=(aligned_memory &&other) = delete;

	byte* get() const noexcept
	{
		return m_data;
	}

private:
	byte *m_data;
};

} // namespace vitrio
