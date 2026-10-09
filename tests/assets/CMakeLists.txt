# SPDX-License-Identifier: LGPL-2.1-or-later

# The files the tests are given are read where they are, in the source tree.
set(VITRIO_TEST_ASSET_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Where the tests write the files they create. It is under the build tree, so
# that a test writes where it is meant to whatever it was run from.
set(VITRIO_TEST_SCRATCH_ROOT ${CMAKE_CURRENT_BINARY_DIR}/scratch)
file(MAKE_DIRECTORY ${VITRIO_TEST_SCRATCH_ROOT})

add_library(vitrio-test-assets-interface INTERFACE)

target_include_directories(
	vitrio-test-assets-interface
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(
	vitrio-test-assets-interface
	INTERFACE
		VITRIO_TEST_ASSET_ROOT="${VITRIO_TEST_ASSET_ROOT}"
		VITRIO_TEST_SCRATCH_ROOT="${VITRIO_TEST_SCRATCH_ROOT}"
)
