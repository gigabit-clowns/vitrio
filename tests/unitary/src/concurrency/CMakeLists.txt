# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_counting_completion.cpp
		test_synchronous_executor.cpp
		test_thread_pool_executor.cpp
)
