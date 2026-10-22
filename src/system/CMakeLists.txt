# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		blocking_file_read_queue.cpp
		file_read_completion.cpp
		file_read_queue.cpp
)

# What is asked of the operating system has one source file per family of
# systems, and the build takes the one that applies.
if(WIN32)
	target_sources(
		vitrio-obj
		PRIVATE
			page_prefetch_windows.cpp
			page_size_windows.cpp
			scattered_file_read_windows.cpp
	)
elseif(APPLE)
	target_sources(
		vitrio-obj
		PRIVATE
			iovec_gathering.cpp
			page_prefetch_darwin.cpp
			page_size_posix.cpp
			scattered_file_read_posix.cpp
	)
elseif(UNIX)
	target_sources(
		vitrio-obj
		PRIVATE
			iovec_gathering.cpp
			page_prefetch_posix.cpp
			page_size_posix.cpp
			scattered_file_read_posix.cpp
	)
else()
	message(FATAL_ERROR "vitrio does not know this operating system.")
endif()

# Reads are queued with the kernel through io_uring only where the build has
# liburing.
if(VITRIO_ENABLE_IO_URING)
	target_sources(
		vitrio-obj
		PRIVATE
			io_uring_file_read_queue.cpp
	)
endif()
