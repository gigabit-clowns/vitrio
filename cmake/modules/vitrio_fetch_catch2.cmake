# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches Catch2 at VERSION, checks it against SHA256 and builds it with the
# project, as Catch2::Catch2WithMain.
#
#   vitrio_fetch_catch2(VERSION <version> SHA256 <hash>)
#
# Catch2 adds the directory of its Catch module, which defines
# catch_discover_tests(), to CMAKE_MODULE_PATH, as its installed package does
# for whoever finds it. That happens in this function's scope, so it is
# handed on to the caller.
function(vitrio_fetch_catch2)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "VERSION;SHA256" "")

	set(tags "https://github.com/catchorg/Catch2/archive/refs/tags")
	FetchContent_Declare(
		Catch2
		URL "${tags}/v${arg_VERSION}.tar.gz"
		URL_HASH SHA256=${arg_SHA256}
	)
	FetchContent_MakeAvailable(Catch2)

	set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()
