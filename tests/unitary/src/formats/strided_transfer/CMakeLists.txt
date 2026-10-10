# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_image_page_regions.cpp
		test_image_region_bounds.cpp
		test_image_region_copy.cpp
		test_image_region_layout.cpp
		test_image_region_loop.cpp
		test_image_region_offsets.cpp
		test_image_region_read_walk.cpp
		test_image_region_transfer.cpp
		test_image_region_write_walk.cpp
)
