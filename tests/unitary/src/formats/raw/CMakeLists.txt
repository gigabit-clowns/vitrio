# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_create_image_file.cpp
		test_open_raw_image_reader.cpp
		test_open_raw_image_writer.cpp
		test_raw_image_layout.cpp
)

add_subdirectory(mapped)
add_subdirectory(planning)
add_subdirectory(queued)
