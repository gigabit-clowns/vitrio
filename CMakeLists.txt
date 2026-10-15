# SPDX-License-Identifier: LGPL-2.1-or-later

cmake_minimum_required(VERSION 3.18)

# Without this CMAKE_MSVC_DEBUG_INFORMATION_FORMAT is silently ignored.
if(POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
endif()

# What a fetched archive holds takes the time it is extracted at, so that a
# new archive always rebuilds what came from it.
if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

project(
	vitrio
	VERSION ${PROJECT_VERSION}
	DESCRIPTION
		"High-throughput image I/O for Cryo-Electron Microscopy (CryoEM)"
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(GenerateExportHeader)
include(vitrio_enable_warnings)

option(VITRIO_INSTALL "Generate vitrio's install rules" ON)
option(
	VITRIO_ENABLE_TIFF 
	"Read and write TIFF and EER files, which needs libtiff"
	ON
)
option(VITRIO_BUILD_PYTHON "Build the Python package, which needs nanobind" OFF)
option(
	VITRIO_PYTHON_STABLE_ABI
	"Build the extension of the Python package against the stable ABI, so \
that one build serves Python 3.12 and newer. Needs CMake 3.26"
	OFF
)
option(VITRIO_BUILD_TESTING "Build vitrio's tests" ${BUILD_TESTING})
option(
	VITRIO_BUILD_DOCS
	"Add a target that builds the documentation of the API, which needs Doxygen"
	OFF
)
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

# Every dependency is private and none appears in a public header, so building
# one here or finding an installed one changes nothing for a consumer of the
# installed library. Turning the global switch off turns every one of the
# others off, on every configure.
option(
	VITRIO_BUILD_DEPENDENCIES
	"Build the dependencies with vitrio, rather than finding installed ones"
	ON
)
cmake_dependent_option(
	VITRIO_BUILD_BOOST "Fetch Boost and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)
cmake_dependent_option(
	VITRIO_BUILD_HALF "Fetch half and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)
cmake_dependent_option(
	VITRIO_BUILD_SPDLOG "Fetch spdlog and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)
cmake_dependent_option(
	VITRIO_BUILD_ZLIB "Fetch zlib and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)
cmake_dependent_option(
	VITRIO_BUILD_TIFF "Fetch libtiff and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
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

if(VITRIO_BUILD_BOOST)
	include(vitrio_fetch_boost)
	vitrio_fetch_boost(
		VERSION 1.92.0
		SHA256 9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d
	)
else()
	find_package(Boost 1.70 REQUIRED CONFIG COMPONENTS filesystem)
endif()

if(VITRIO_BUILD_HALF)
	include(vitrio_fetch_half)
	vitrio_fetch_half(
		VERSION 2.2.1
		SHA256 76ddbf406e9d9b772ec73af2bf925b38b290b4390cc4064720a08d4b4bca0aa9
	)
else()
	find_package(half REQUIRED)
endif()

if(VITRIO_BUILD_SPDLOG)
	include(vitrio_fetch_spdlog)
	vitrio_fetch_spdlog(
		VERSION 1.17.0
		SHA256 d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
	)
else()
	find_package(spdlog 1.4 REQUIRED CONFIG)
endif()

# Only the TIFF and EER formats need libtiff, and zlib is libtiff's. libtiff
# looks zlib up itself, so zlib comes first: built here, it is the one
# libtiff finds.
if(VITRIO_ENABLE_TIFF AND VITRIO_BUILD_ZLIB)
	include(vitrio_fetch_zlib)
	vitrio_fetch_zlib(
		VERSION 1.3.2
		SHA256 d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3
	)
elseif(VITRIO_ENABLE_TIFF)
	find_package(ZLIB REQUIRED)
endif()

# The handlers a file is opened with, which keep its errors apart from those
# of every other file, arrived in libtiff 4.5.
if(VITRIO_ENABLE_TIFF AND VITRIO_BUILD_TIFF)
	include(vitrio_fetch_tiff)
	vitrio_fetch_tiff(
		VERSION 4.7.2
		SHA256 4996f0c4f93094719b1ca5c6279b20e588773ba8a247533e486416fb662ddb88
	)
elseif(VITRIO_ENABLE_TIFF)
	find_package(TIFF 4.5 REQUIRED)
endif()

# nanobind ships its CMake package inside its Python package, so the
# interpreter that has it is asked where that is.
if(VITRIO_BUILD_PYTHON)
	set(VITRIO_PYTHON_COMPONENTS Interpreter Development.Module)
	if(VITRIO_PYTHON_STABLE_ABI)
		list(APPEND VITRIO_PYTHON_COMPONENTS Development.SABIModule)
	endif()

	find_package(
		Python 3.10 REQUIRED
		COMPONENTS ${VITRIO_PYTHON_COMPONENTS}
	)

	if(NOT nanobind_ROOT AND NOT nanobind_DIR)
		execute_process(
			COMMAND ${Python_EXECUTABLE} -m nanobind --cmake_dir
			OUTPUT_VARIABLE nanobind_ROOT
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
	endif()

	find_package(nanobind 3.0 REQUIRED CONFIG)
endif()

add_subdirectory(src)

if(VITRIO_BUILD_PYTHON)
	add_subdirectory(python)
endif()

if(BUILD_TESTING AND VITRIO_BUILD_TESTING)
	add_subdirectory(tests)
endif()

if(VITRIO_CHECK_HEADERS)
	add_subdirectory(tests/headers)
endif()

if(VITRIO_BUILD_DOCS)
	add_subdirectory(docs)
endif()
