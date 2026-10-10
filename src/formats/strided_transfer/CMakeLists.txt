# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		image_page_regions.cpp
		image_region_bounds.cpp
		image_region_copy.cpp
		image_region_layout.cpp
		image_region_offsets.cpp
		image_region_read_walk.cpp
		image_region_transfer.cpp
		image_region_transfer_complex_float16.cpp
		image_region_transfer_complex_float32.cpp
		image_region_transfer_complex_float64.cpp
		image_region_transfer_float16.cpp
		image_region_transfer_float32.cpp
		image_region_transfer_float64.cpp
		image_region_transfer_int16.cpp
		image_region_transfer_int32.cpp
		image_region_transfer_int64.cpp
		image_region_transfer_int8.cpp
		image_region_transfer_uint16.cpp
		image_region_transfer_uint32.cpp
		image_region_transfer_uint64.cpp
		image_region_transfer_uint8.cpp
		image_region_write_walk.cpp
)
