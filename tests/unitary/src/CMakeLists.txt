# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_index_table.cpp
		test_library_version.cpp
		test_span.cpp
		test_version.cpp
)

add_subdirectory(array)
add_subdirectory(concurrency)
add_subdirectory(memory)
