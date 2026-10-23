# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		file_transfer_dispatch.cpp
		file_transfer_executor.cpp
		file_transfer_inbox.cpp
		file_transfer_operation.cpp
		file_transfer_scope.cpp
		file_transfer_task_notifier.cpp
		file_transfer_worker.cpp
		thread_pool_file_transfer_executor.cpp
)
