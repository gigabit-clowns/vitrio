# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches c-blosc at COMMIT, that of a release, and builds it with the project,
# static, as blosc::blosc, over the zstd and zlib of the targets ZSTD and ZLIB.
#
#   vitrio_fetch_blosc(ZSTD <target> ZLIB <target> COMMIT <sha>)
#
# c-blosc carries copies of zstd and zlib, which would define their functions
# a second time beside the libraries the project links, so it is given those
# instead: its search for each stops at the variables set here, and it links
# what they name. Its copy of lz4 stays, as nothing else has lz4. Snappy, its
# programs, tests, fuzzers and benchmarks are left out.
function(vitrio_fetch_blosc)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT;ZSTD;ZLIB" "")

	foreach(name ZSTD ZLIB)
		get_target_property(aliased ${arg_${name}} ALIASED_TARGET)
		if(aliased)
			set(${name}_TARGET ${aliased})
		else()
			set(${name}_TARGET ${arg_${name}})
		endif()
	endforeach()

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	# c-blosc asks for CMake 2.8.12, which CMake 4 no longer accepts as a
	# minimum; this raises it for c-blosc alone. CMake before 3.31 ignores it,
	# and still accepts what c-blosc asks for.
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(BUILD_STATIC ON)
	set(BUILD_SHARED OFF)
	set(BUILD_TESTS OFF)
	set(BUILD_FUZZERS OFF)
	set(BUILD_BENCHMARKS OFF)
	set(DEACTIVATE_SNAPPY ON)
	set(PREFER_EXTERNAL_ZSTD ON)
	set(PREFER_EXTERNAL_ZLIB ON)
	set(
		ZSTD_INCLUDE_DIR
		"$<TARGET_PROPERTY:${ZSTD_TARGET},INTERFACE_INCLUDE_DIRECTORIES>"
		CACHE STRING "Where the headers of zstd are, for c-blosc" FORCE
	)
	set(
		ZSTD_LIBRARY ${arg_ZSTD}
		CACHE STRING "The zstd c-blosc links" FORCE
	)
	set(
		ZLIB_INCLUDE_DIR
		"$<TARGET_PROPERTY:${ZLIB_TARGET},INTERFACE_INCLUDE_DIRECTORIES>"
		CACHE STRING "Where the headers of zlib are, for c-blosc" FORCE
	)
	set(
		ZLIB_LIBRARY ${arg_ZLIB}
		CACHE STRING "The zlib c-blosc links" FORCE
	)

	FetchContent_Declare(
		blosc
		URL "https://github.com/Blosc/c-blosc/archive/${arg_COMMIT}.tar.gz"
	)
	FetchContent_MakeAvailable(blosc)

	add_library(vitrio-blosc INTERFACE)
	target_link_libraries(
		vitrio-blosc
		INTERFACE
			blosc_static
			${arg_ZSTD}
			${arg_ZLIB}
	)
	target_include_directories(
		vitrio-blosc
		SYSTEM INTERFACE
			$<BUILD_INTERFACE:${blosc_SOURCE_DIR}/blosc>
	)
	add_library(blosc::blosc ALIAS vitrio-blosc)
endfunction()
