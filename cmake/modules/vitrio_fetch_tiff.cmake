# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches libtiff at VERSION, checks it against SHA256 and builds it with the
# project, static, as TIFF::TIFF.
#
#   vitrio_fetch_tiff(VERSION <version> SHA256 <hash>)
#
# Only what reads and writes TIFF and EER files is built: its tools, tests,
# documentation and every codec but zlib are left out, so that nothing else
# on the machine is picked up. libtiff names its library TIFF::tiff, where
# FindTIFF names it TIFF::TIFF.
#
# libtiff states the policies of CMake 3.10, before options took the value
# of a variable of the same name, so that is asked for here.
function(vitrio_fetch_tiff)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "VERSION;SHA256" "")

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_SHARED_LIBS OFF)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	set(tiff-tools OFF)
	set(tiff-tests OFF)
	set(tiff-contrib OFF)
	set(tiff-docs OFF)
	set(tiff-cxx OFF)
	set(jbig OFF)
	set(jpeg OFF)
	set(old-jpeg OFF)
	set(lerc OFF)
	set(libdeflate OFF)
	set(lzma OFF)
	set(webp OFF)
	set(zstd OFF)

	FetchContent_Declare(
		TIFF
		URL "https://download.osgeo.org/libtiff/tiff-${arg_VERSION}.tar.xz"
		URL_HASH SHA256=${arg_SHA256}
	)
	FetchContent_MakeAvailable(TIFF)

	add_library(TIFF::TIFF ALIAS tiff)
endfunction()
