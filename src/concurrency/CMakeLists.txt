# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		counting_completion.cpp
		counting_notifier.cpp
		failure_forwarding_notifier.cpp
		synchronous_executor.cpp
		thread_pool_executor.cpp
)
