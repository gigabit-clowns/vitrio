# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		open_raw_image_reader.cpp
		raw_image_layout.cpp
)

add_subdirectory(mapped)
add_subdirectory(planning)
add_subdirectory(queued)
