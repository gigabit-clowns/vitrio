# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		create_image_file.cpp
		open_raw_image_reader.cpp
		open_raw_image_writer.cpp
		raw_image_layout.cpp
)

if(VITRIO_ENABLE_GZIP)
	add_subdirectory(gzip)
endif()

add_subdirectory(mapped)
add_subdirectory(planning)
add_subdirectory(queued)
