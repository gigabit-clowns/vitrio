# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches zlib at COMMIT, that of a release, and builds it with the project,
# static, as ZLIB::ZLIB.
#
#   vitrio_fetch_zlib(COMMIT <sha>)
#
# libtiff looks zlib up with a find_package of its own, which
# OVERRIDE_FIND_PACKAGE answers with this build. That arrived in CMake 3.24.
# Built static, zlib names its library ZLIB::ZLIBSTATIC rather than the
# ZLIB::ZLIB libtiff links.
function(vitrio_fetch_zlib)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

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

	FetchContent_Declare(
		ZLIB
		URL "https://github.com/madler/zlib/archive/${arg_COMMIT}.tar.gz"
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(ZLIB)

	add_library(ZLIB::ZLIB ALIAS zlibstatic)
endfunction()
