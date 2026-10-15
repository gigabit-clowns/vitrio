# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches zlib at VERSION, checks it against SHA256 and builds it with the
# project, static, as ZLIB::ZLIB.
#
#   vitrio_fetch_zlib(VERSION <version> SHA256 <hash>)
#
# libtiff looks zlib up with a find_package of its own, which
# OVERRIDE_FIND_PACKAGE answers with this build. That arrived in CMake 3.24.
# Built static, zlib names its library ZLIB::ZLIBSTATIC rather than the
# ZLIB::ZLIB libtiff links.
function(vitrio_fetch_zlib)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "VERSION;SHA256" "")

	if(CMAKE_VERSION VERSION_LESS 3.24)
		message(
			FATAL_ERROR
			"Building zlib needs CMake 3.24. Turn VITRIO_BUILD_ZLIB off or "
			"use a newer CMake."
		)
	endif()

	set(ZLIB_BUILD_SHARED OFF)
	set(ZLIB_BUILD_TESTING OFF)
	set(ZLIB_INSTALL OFF)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)

	set(releases "https://github.com/madler/zlib/releases/download")
	set(archive "v${arg_VERSION}/zlib-${arg_VERSION}.tar.xz")
	FetchContent_Declare(
		ZLIB
		URL "${releases}/${archive}"
		URL_HASH SHA256=${arg_SHA256}
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(ZLIB)

	add_library(ZLIB::ZLIB ALIAS zlibstatic)
endfunction()
