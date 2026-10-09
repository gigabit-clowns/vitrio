# SPDX-License-Identifier: LGPL-2.1-or-later

# The integration tests are black-box: they are linked with the shared
# library and see only its public headers, as a consumer does.
add_executable(vitrio-integration-tests)

add_subdirectory(src)

target_compile_definitions(
	vitrio-integration-tests
	PRIVATE
		VITRIO_TEST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		VITRIO_TEST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		VITRIO_TEST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(
	vitrio-integration-tests
	PRIVATE
		vitrio
		Catch2::Catch2WithMain
)

# Windows looks for a library beside the executable that needs it.
if(WIN32)
	add_custom_command(
		TARGET vitrio-integration-tests
		POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy
			$<TARGET_FILE:vitrio>
			$<TARGET_FILE_DIR:vitrio-integration-tests>
	)
endif()

vitrio_enable_warnings(vitrio-integration-tests)
vitrio_register_tests(vitrio-integration-tests integration)
