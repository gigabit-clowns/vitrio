# SPDX-License-Identifier: LGPL-2.1-or-later

include(FetchContent)

# liburing is written in C, which the project does not otherwise compile.
# CMake enables a language only outside a function, so it is enabled here,
# where the module is included.
enable_language(C)

# Fetches liburing at COMMIT, that of a release, and builds it with the
# project, static, as liburing::liburing.
#
#   vitrio_fetch_liburing(COMMIT <sha>)
#
# liburing has no CMake build: a configure script of its own writes the
# headers that say what the C library and the kernel's headers offer, and
# make compiles it. The script is run here, into the build tree, which needs
# a POSIX shell and make, and the library is built as its Makefile builds it,
# against the C library. The script is run again only when a new archive
# brings a new one.
function(vitrio_fetch_liburing)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "COMMIT" "")

	FetchContent_Declare(
		liburing
		URL "https://github.com/axboe/liburing/archive/${arg_COMMIT}.tar.gz"
	)
	FetchContent_MakeAvailable(liburing)

	set(source ${liburing_SOURCE_DIR})
	set(build ${liburing_BINARY_DIR})

	if(${source}/configure IS_NEWER_THAN ${build}/config-host.h)
		execute_process(
			COMMAND
				sh ${source}/configure
				--use-libc
				--cc=${CMAKE_C_COMPILER}
				--cxx=${CMAKE_CXX_COMPILER}
			WORKING_DIRECTORY ${build}
			RESULT_VARIABLE status
			OUTPUT_VARIABLE output
			ERROR_VARIABLE output
		)
		if(NOT status EQUAL 0)
			message(
				FATAL_ERROR
				"The configure script of liburing failed:\n${output}"
			)
		endif()
	endif()

	add_library(
		uring
		STATIC
			${source}/src/queue.c
			${source}/src/register.c
			${source}/src/setup.c
			${source}/src/syscall.c
			${source}/src/version.c
	)

	set_target_properties(uring PROPERTIES POSITION_INDEPENDENT_CODE ON)

	# The headers configure writes sit beside those of the archive.
	target_include_directories(
		uring
		SYSTEM PUBLIC
			${source}/src/include
			${build}/src/include
	)

	target_compile_definitions(
		uring
		PRIVATE
			_GNU_SOURCE
			_LARGEFILE_SOURCE
			_FILE_OFFSET_BITS=64
			LIBURING_INTERNAL
	)

	target_compile_options(
		uring
		PRIVATE
			"SHELL:-include \"${build}/config-host.h\""
	)

	add_library(liburing::liburing ALIAS uring)
endfunction()
