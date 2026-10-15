# SPDX-License-Identifier: LGPL-2.1-or-later

# Declared here rather than beside the other toggles, because these two
# dependencies exist only while the tests are built.
cmake_dependent_option(
	VITRIO_BUILD_CATCH2 "Fetch Catch2 and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)
cmake_dependent_option(
	VITRIO_BUILD_TROMPELOEIL "Fetch trompeloeil and build it with vitrio" ON
	"VITRIO_BUILD_DEPENDENCIES" OFF
)

if(VITRIO_BUILD_CATCH2)
	include(vitrio_fetch_catch2)
	vitrio_fetch_catch2(
		COMMIT 08092139210881f01a34e60c5f2fd6afa0ea4024
	) # v3.16.1
else()
	find_package(Catch2 3 REQUIRED CONFIG)
endif()

if(VITRIO_BUILD_TROMPELOEIL)
	include(vitrio_fetch_trompeloeil)
	vitrio_fetch_trompeloeil(
		COMMIT eaeb89c1ce9d354b0ba7eb921fd5712cdbd78adf
	) # v49
else()
	find_package(trompeloeil REQUIRED CONFIG)
endif()

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

add_subdirectory(assets)
add_subdirectory(unitary)
add_subdirectory(integration)

if(VITRIO_BUILD_PYTHON)
	add_subdirectory(python)
endif()

if(VITRIO_INSTALL)
	add_subdirectory(install)
endif()
