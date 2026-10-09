// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "host_ndarray.hpp"

#include <future>
#include <memory>

namespace vitrio
{

/**
 * @brief The loan of the memory of an array given from Python.
 *
 * A loan keeps that memory alive. The arrays made over the memory are given
 * the owner of the loan, which owns nothing: it only tells the loan when
 * the last of those arrays is gone. So a thread that destroys such an array
 * never releases anything of Python, and never needs the GIL.
 *
 * Destroying a loan waits until every array given its owner is gone, and
 * only then lets go of the memory. The GIL is released during the wait. A
 * loan is destroyed by a thread that holds the GIL.
 *
 * @see import_array
 */
class memory_loan
{
public:
	/**
	 * @brief Construct the loan of the memory of an array.
	 *
	 * @param memory The array given from Python.
	 */
	explicit memory_loan(const_host_ndarray memory);
	memory_loan(const memory_loan &other) = delete;
	memory_loan(memory_loan &&other) noexcept;
	~memory_loan();

	memory_loan& operator=(const memory_loan &other) = delete;
	memory_loan& operator=(memory_loan &&other) = delete;

	/**
	 * @brief Get the owner to give to an array over the memory.
	 *
	 * @return std::shared_ptr<void> The owner. Null once the loan has been
	 * moved from.
	 */
	std::shared_ptr<void> get_owner() const noexcept;

private:
	const_host_ndarray m_memory;
	std::shared_ptr<std::promise<void>> m_owner;
	std::future<void> m_returned;
};

} // namespace vitrio
