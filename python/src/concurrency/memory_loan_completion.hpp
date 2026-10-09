// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array/memory_loan.hpp>

#include <vitrio/concurrency/completion.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief The completion of work done over memory on loan from Python.
 *
 * It reports the work as the completion it was given does, and it holds the
 * loan for as long as it lives. Destroying it ends the loan, which waits
 * for the work to let go of the memory. It is destroyed by a thread that
 * holds the GIL.
 *
 * @see memory_loan
 */
class memory_loan_completion final
	: public completion
{
public:
	/**
	 * @brief Construct the completion of work done over memory on loan.
	 *
	 * @param work The completion of the work. Must not be null.
	 * @param loan The loan of the memory the work reads or writes.
	 * @throws std::invalid_argument If @p work is null.
	 */
	memory_loan_completion(
		std::shared_ptr<completion> work,
		memory_loan loan
	);
	memory_loan_completion(const memory_loan_completion &other) = delete;
	memory_loan_completion(memory_loan_completion &&other) = delete;
	~memory_loan_completion() override;

	memory_loan_completion&
	operator=(const memory_loan_completion &other) = delete;
	memory_loan_completion&
	operator=(memory_loan_completion &&other) = delete;

	void wait() const override;
	bool is_ready() const noexcept override;
	void get() const override;

private:
	// The work may hold an array over the memory, so it goes first.
	memory_loan m_loan;
	std::shared_ptr<completion> m_work;
};

} // namespace vitrio
