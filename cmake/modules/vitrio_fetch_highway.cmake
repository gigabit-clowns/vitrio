# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# Fetches Google Highway at COMMIT, that of a release, and builds it with the
# project, static, as hwy::hwy.
#
#   vitrio_fetch_highway(COMMIT <sha>)
#
# Only the library is built: neither its contrib, which is what needs C++17,
# nor its examples, its tests or its install rules. Highway declares its
# switches as cache entries rather than options, so the policy that lets a
# plain variable stand in for one is CMP0126, not CMP0077 alone. It compiles
# its own sources as C++17 when it may, and warns when told the standard of
# the project is lower, so the standard is left for it to choose: what it
# asks of what links it is C++11. Its build tree names no hwy:: target, only
# its installed package does, so the alias is made here, over a target that
# marks its headers as system ones, so that the warnings of the project do
# not fire inside them.
function(vitrio_fetch_highway)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	unset(CMAKE_CXX_STANDARD)
	set(BUILD_SHARED_LIBS OFF)
	set(HWY_FORCE_STATIC_LIBS ON)
	set(HWY_ENABLE_CONTRIB OFF)
	set(HWY_ENABLE_EXAMPLES OFF)
	set(HWY_ENABLE_INSTALL OFF)
	set(HWY_ENABLE_TESTS OFF)
	set(HWY_WARNINGS_ARE_ERRORS OFF)

	FetchContent_Declare(
		highway
		URL "https://github.com/google/highway/archive/${arg_COMMIT}.tar.gz"
	)
	FetchContent_MakeAvailable(highway)

	add_library(vitrio-highway INTERFACE)
	target_link_libraries(vitrio-highway INTERFACE hwy)
	target_include_directories(
		vitrio-highway
		SYSTEM INTERFACE
			$<BUILD_INTERFACE:${highway_SOURCE_DIR}>
	)
	add_library(hwy::hwy ALIAS vitrio-highway)
endfunction()
