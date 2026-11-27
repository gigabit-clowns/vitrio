# SPDX-License-Identifier: LGPL-2.1-or-later

# Compiling an example keeps it in step with the API. Running it is cheap,
# and it checks what it prints, so it is a test too.
foreach(example read-and-convert read-batches)
	string(REPLACE "-" "_" source ${example})
	add_executable(vitrio-example-${example} ${source}.cpp)

	target_link_libraries(
		vitrio-example-${example}
		PRIVATE
			vitrio::vitrio
	)

	if(VITRIO_BUILD_TESTING)
		add_test(
			NAME vitrio-example-${example}
			COMMAND vitrio-example-${example}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		)
		set_tests_properties(
			vitrio-example-${example}
			PROPERTIES
				LABELS example
		)
	endif()
endforeach()
