# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-python
	PRIVATE
		completion.cpp
		executor.cpp
		memory_loan_completion.cpp
		synchronous_executor.cpp
		thread_pool_executor.cpp
)
