# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_chunk_regions.cpp
		test_page_regions.cpp
		test_region_bounds.cpp
		test_region_offsets.cpp
		test_region_read_walk.cpp
		test_region_transfer.cpp
		test_region_write_walk.cpp
)

add_subdirectory(region_loop)

if(VITRIO_ENABLE_SIMD)
	add_subdirectory(simd)
endif()
