# SPDX-License-Identifier: LGPL-2.1-or-later

add_executable(vitrio-example-read-and-convert read_and_convert.cpp)

target_link_libraries(
	vitrio-example-read-and-convert
	PRIVATE
		vitrio::vitrio
)

# Compiling it keeps the example in step with the API. Running it is cheap,
# and it checks what it prints, so it is a test too.
if(BUILD_TESTING AND VITRIO_BUILD_TESTING)
	add_test(
		NAME vitrio-example-read-and-convert
		COMMAND vitrio-example-read-and-convert
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	set_tests_properties(
		vitrio-example-read-and-convert
		PROPERTIES
			LABELS example
	)
endif()
