# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		chunk_regions.cpp
		page_regions.cpp
		region_bounds.cpp
		region_offsets.cpp
		region_read_walk.cpp
		region_transfer.cpp
		region_transfer_boolean.cpp
		region_transfer_complex_float16.cpp
		region_transfer_complex_float32.cpp
		region_transfer_complex_float64.cpp
		region_transfer_float16.cpp
		region_transfer_float32.cpp
		region_transfer_float64.cpp
		region_transfer_int16.cpp
		region_transfer_int32.cpp
		region_transfer_int64.cpp
		region_transfer_int8.cpp
		region_transfer_uint16.cpp
		region_transfer_uint32.cpp
		region_transfer_uint64.cpp
		region_transfer_uint8.cpp
		region_write_walk.cpp
)

add_subdirectory(region_loop)

if(VITRIO_ENABLE_SIMD)
	add_subdirectory(simd)
endif()
