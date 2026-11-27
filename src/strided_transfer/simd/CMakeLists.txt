# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		float16_region_transfer.cpp
)

target_link_libraries(vitrio-obj PUBLIC hwy::hwy)

# Highway reads which instruction sets to compile for from macros, which the
# tests have to see as the library does, so they are public to its objects.
# They reach no consumer: the shared library takes the objects privately.
# VITRIO_SIMD_DISABLED_TARGETS is worked out beside the option, at the top.
target_compile_definitions(
	vitrio-obj
	PUBLIC
		VITRIO_ENABLE_SIMD
		"HWY_DISABLED_TARGETS=${VITRIO_SIMD_DISABLED_TARGETS}"
)
if(VITRIO_SIMD_DISPATCH STREQUAL STATIC)
	target_compile_definitions(vitrio-obj PUBLIC HWY_COMPILE_ONLY_STATIC)
endif()
