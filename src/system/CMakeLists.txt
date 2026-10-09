# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		memory_range.cpp
)

# What is asked of the operating system has one source file per family of
# systems, and the build takes the one that applies.
if(WIN32)
	target_sources(
		vitrio-obj
		PRIVATE
			page_prefetch_windows.cpp
			page_size_windows.cpp
	)
elseif(UNIX)
	target_sources(
		vitrio-obj
		PRIVATE
			page_prefetch_posix.cpp
			page_size_posix.cpp
	)
else()
	message(FATAL_ERROR "vitrio does not know this operating system.")
endif()
