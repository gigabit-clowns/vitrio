# SPDX-License-Identifier: LGPL-2.1-or-later

# The unit tests are white-box: they are linked with the objects of the
# library rather than with the library, and see its private headers.
add_executable(vitrio-unitary-tests)

add_subdirectory(src)

# The objects are in this executable, so nothing is imported from a shared
# library.
target_compile_definitions(
	vitrio-unitary-tests
	PRIVATE
		VITRIO_NO_EXPORTS
		VITRIO_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		VITRIO_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		VITRIO_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_include_directories(
	vitrio-unitary-tests
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
	vitrio-unitary-tests
	PRIVATE
		vitrio-obj
		vitrio-test-assets-interface
		Catch2::Catch2WithMain
		trompeloeil::trompeloeil
)

# The mocks count their arguments with a macro that only the conforming
# preprocessor of MSVC expands as the others do.
#
# The tests of the element types use complex numbers of half precision, as
# the library does. See src/CMakeLists.txt for the definition that goes with
# them.
if(MSVC)
	target_compile_options(
		vitrio-unitary-tests
		PRIVATE
			/Zc:preprocessor
	)
	target_compile_definitions(
		vitrio-unitary-tests
		PRIVATE
			_SILENCE_NONFLOATING_COMPLEX_DEPRECATION_WARNING
	)
endif()

vitrio_enable_warnings(vitrio-unitary-tests)
vitrio_register_tests(vitrio-unitary-tests unitary)
