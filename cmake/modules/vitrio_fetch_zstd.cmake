# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches zstd at COMMIT, that of a release, and builds it with the project,
# static, as zstd::zstd.
#
#   vitrio_fetch_zstd(COMMIT <sha>)
#
# zstd keeps its CMake build in build/cmake. Only the library is built, with
# neither its programs, its tests nor its support of the formats of releases
# before 0.8. It is single threaded: what calls it runs on threads of its own.
# Whether its library states where its headers are differs between releases,
# so that is stated here.
function(vitrio_fetch_zstd)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(ZSTD_BUILD_PROGRAMS OFF)
	set(ZSTD_BUILD_TESTS OFF)
	set(ZSTD_BUILD_CONTRIB OFF)
	set(ZSTD_BUILD_SHARED OFF)
	set(ZSTD_BUILD_STATIC ON)
	set(ZSTD_LEGACY_SUPPORT OFF)
	set(ZSTD_MULTITHREAD_SUPPORT OFF)

	FetchContent_Declare(
		zstd
		URL "https://github.com/facebook/zstd/archive/${arg_COMMIT}.tar.gz"
		SOURCE_SUBDIR build/cmake
	)
	FetchContent_MakeAvailable(zstd)

	add_library(vitrio-zstd INTERFACE)
	target_link_libraries(vitrio-zstd INTERFACE libzstd_static)
	target_include_directories(
		vitrio-zstd
		SYSTEM INTERFACE
			$<BUILD_INTERFACE:${zstd_SOURCE_DIR}/lib>
	)
	add_library(zstd::zstd ALIAS vitrio-zstd)
endfunction()
