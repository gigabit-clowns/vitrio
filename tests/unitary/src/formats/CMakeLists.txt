# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_open_raw_image_reader.cpp
)

add_subdirectory(dm)
add_subdirectory(memory_mapping)
add_subdirectory(mrc)
add_subdirectory(read_planning)
add_subdirectory(strided_transfer)

if(VITRIO_ENABLE_TIFF)
	add_subdirectory(eer)
	add_subdirectory(tiff)
endif()
