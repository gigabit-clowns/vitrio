# SPDX-License-Identifier: LGPL-2.1-or-later

cmake_minimum_required(VERSION 3.18)

# Without this CMAKE_MSVC_DEBUG_INFORMATION_FORMAT is silently ignored.
if(POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
endif()

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

project(
	vitrio
	VERSION ${PROJECT_VERSION}
	DESCRIPTION "Reads and writes the image files of electron microscopy"
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(vitrio_enable_warnings)

option(VITRIO_INSTALL "Generate vitrio's install rules" ON)
option(VITRIO_BUILD_TESTING "Build vitrio's tests" ${BUILD_TESTING})
option(
	VITRIO_REGISTER_TESTS_PER_BINARY
	"Register one test per test executable instead of one per Catch2 case"
	OFF
)
option(
	VITRIO_CHECK_HEADERS
	"Compile every public header on its own under each C++ standard the \
compiler supports"
	OFF
)

# The library and its public headers are C++14. The standard is required
# rather than preferred, so that nothing newer gets in unnoticed.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows looks for a library beside the executable that needs it, and has no
# other way to be told where one is. So the library, the libraries it needs
# in turn and the test executables are all built into one directory.
if(WIN32 AND NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()

find_package(Threads REQUIRED)
find_package(Boost 1.70 REQUIRED CONFIG COMPONENTS filesystem)
find_package(half REQUIRED)

add_subdirectory(src)

if(BUILD_TESTING AND VITRIO_BUILD_TESTING)
	add_subdirectory(tests)
endif()

if(VITRIO_CHECK_HEADERS)
	add_subdirectory(tests/headers)
endif()
