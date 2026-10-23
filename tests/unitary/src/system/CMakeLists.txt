# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_asynchronous_file_read_queue_pool.cpp
		test_blocking_file_read_port.cpp
		test_blocking_file_read_queue.cpp
		test_file_read_completion.cpp
		test_page_prefetch.cpp
		test_page_size.cpp
		test_scattered_file_read.cpp
)

if(NOT WIN32)
	target_sources(
		vitrio-unitary-tests
		PRIVATE
			test_iovec_gathering.cpp
	)
endif()

if(VITRIO_ENABLE_IO_URING)
	target_sources(
		vitrio-unitary-tests
		PRIVATE
			test_event_counter.cpp
			test_io_uring_file_read_port.cpp
			test_io_uring_file_read_queue.cpp
			test_unseen_mapping_annotation.cpp
	)
endif()
