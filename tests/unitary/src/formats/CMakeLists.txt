# SPDX-License-Identifier: LGPL-2.1-or-later

add_subdirectory(memory_mapping)
add_subdirectory(mrc)
add_subdirectory(strided_transfer)

if(VITRIO_ENABLE_TIFF)
	add_subdirectory(tiff)
endif()
