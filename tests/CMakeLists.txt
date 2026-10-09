# SPDX-License-Identifier: LGPL-2.1-or-later

find_package(Catch2 3 REQUIRED CONFIG)
include(Catch)

# Registers the cases of a test executable with CTest, either one by one or
# as a single test that runs them all. The single test is what a run under a
# slow tool wants, since it starts the executable once.
#
# Catch2 lists the cases through CMake's JSON support, which CMake 3.18 does
# not have, so there the single test is the only choice.
function(vitrio_register_tests TARGET LABEL)
	if(VITRIO_REGISTER_TESTS_PER_BINARY OR CMAKE_VERSION VERSION_LESS 3.19)
		add_test(NAME ${TARGET} COMMAND ${TARGET})
		set_tests_properties(
			${TARGET}
			PROPERTIES
				TIMEOUT 3600
				LABELS ${LABEL}
		)
	else()
		catch_discover_tests(${TARGET} PROPERTIES LABELS ${LABEL})
	endif()
endfunction()

add_subdirectory(unitary)
add_subdirectory(integration)
