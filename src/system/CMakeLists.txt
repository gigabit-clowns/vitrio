# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		asynchronous_file_read_port_opener.cpp
		blocking_file_read_port.cpp
		blocking_file_read_port_opener.cpp
		blocking_file_read_queue.cpp
		file_read_completion.cpp
		file_read_port.cpp
		file_read_port_opener.cpp
		file_read_queue.cpp
		file_read_submitter.cpp
		unseen_mapping_annotation.cpp
)

# What is asked of the operating system has one source file per family of
# systems, and the build takes the one that applies.
if(WIN32)
	target_sources(
		vitrio-obj
		PRIVATE
			environment_variable_windows.cpp
			page_prefetch_windows.cpp
			page_size_windows.cpp
			scattered_file_read_windows.cpp
	)
elseif(APPLE)
	target_sources(
		vitrio-obj
		PRIVATE
			environment_variable_posix.cpp
			iovec_gathering.cpp
			page_prefetch_darwin.cpp
			page_size_posix.cpp
			scattered_file_read_posix.cpp
	)
elseif(UNIX)
	target_sources(
		vitrio-obj
		PRIVATE
			environment_variable_posix.cpp
			iovec_gathering.cpp
			page_prefetch_posix.cpp
			page_size_posix.cpp
			scattered_file_read_posix.cpp
	)
else()
	message(FATAL_ERROR "vitrio does not know this operating system.")
endif()

# Reads are queued with the kernel through io_uring only where the build has
# liburing, which the opener of asynchronous ports is told of.
if(VITRIO_ENABLE_IO_URING)
	target_sources(
		vitrio-obj
		PRIVATE
			event_counter.cpp
			io_uring_file_read_port.cpp
			io_uring_file_read_queue.cpp
	)

	target_compile_definitions(
		vitrio-obj
		PRIVATE
			VITRIO_ENABLE_IO_URING
	)
endif()
