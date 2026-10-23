# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_file_transfer_dispatch.cpp
		test_file_transfer_inbox.cpp
		test_file_transfer_task_notifier.cpp
		test_file_transfer_worker.cpp
		test_thread_pool_file_transfer_executor.cpp
)
