# SPDX-License-Identifier: LGPL-2.1-or-later

# nanobind needs C++17. Only the extension is built with it: the library and
# its headers stay C++14.
set(CMAKE_CXX_STANDARD 17)

# The package is assembled here as it is laid out once installed, the
# extension beside the Python sources, so that the build tree is imported
# with this directory on the path.
set(VITRIO_PYTHON_PACKAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/vitrio)

# nanobind falls back to the ABI of the interpreter on a Python older than
# 3.12, whose stable ABI lacks what it needs.
if(VITRIO_PYTHON_STABLE_ABI)
	nanobind_add_module(vitrio-python STABLE_ABI)
else()
	nanobind_add_module(vitrio-python)
endif()

set_target_properties(
	vitrio-python
	PROPERTIES
		OUTPUT_NAME _binding
		LIBRARY_OUTPUT_DIRECTORY ${VITRIO_PYTHON_PACKAGE_DIR}
)

# A generator of several configurations puts each in a directory of its own
# unless it is told where that configuration goes, which would take the
# extension out of its package.
foreach(CONFIGURATION IN LISTS CMAKE_CONFIGURATION_TYPES)
	string(TOUPPER ${CONFIGURATION} CONFIGURATION)
	set_target_properties(
		vitrio-python
		PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY_${CONFIGURATION}
				${VITRIO_PYTHON_PACKAGE_DIR}
	)
endforeach()

target_link_libraries(
	vitrio-python
	PRIVATE
		vitrio::vitrio
)

# Once installed the library is beside the extension, which is where the
# extension looks for it. Windows looks there without being told.
if(APPLE)
	set_target_properties(
		vitrio-python
		PROPERTIES
			INSTALL_RPATH "@loader_path"
	)
elseif(UNIX)
	set_target_properties(
		vitrio-python
		PROPERTIES
			INSTALL_RPATH "$ORIGIN"
	)
endif()

vitrio_enable_warnings(vitrio-python)

# Windows looks for the libraries an extension needs beside the extension,
# and has no other way to be told where they are. The library is built
# where everything it needs in turn already is, so all of it is copied.
if(WIN32)
	add_custom_command(
		TARGET vitrio-python
		POST_BUILD
		COMMAND
			${CMAKE_COMMAND}
			-DSOURCE_DIR=$<TARGET_FILE_DIR:vitrio>
			-DDESTINATION_DIR=${VITRIO_PYTHON_PACKAGE_DIR}
			-P ${PROJECT_SOURCE_DIR}/cmake/scripts/copy_dynamic_libraries.cmake
		VERBATIM
	)
endif()

add_subdirectory(src)
add_subdirectory(vitrio)

# The stubs are written by importing the extension, so they need the whole
# package in place.
nanobind_add_stub(
	vitrio-python-stubs
	MODULE vitrio._binding
	OUTPUT ${VITRIO_PYTHON_PACKAGE_DIR}/_binding.pyi
	PYTHON_PATH ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS vitrio-python
)

# The package is installed as a component of its own, which a plain install
# leaves out: it goes where Python looks for packages, and that is not under
# the prefix the library goes to. pip installs it, or
#
#   cmake --install <build> --component vitrio_python --prefix <site-packages>
#
# The library goes beside the extension, under the name the extension asks
# for. The import library of Windows belongs to a component nobody installs.
install(
	TARGETS vitrio-python
	LIBRARY
		DESTINATION vitrio
		COMPONENT vitrio_python
		EXCLUDE_FROM_ALL
)

install(
	TARGETS vitrio
	RUNTIME
		DESTINATION vitrio
		COMPONENT vitrio_python
		EXCLUDE_FROM_ALL
	LIBRARY
		DESTINATION vitrio
		COMPONENT vitrio_python
		NAMELINK_SKIP
		EXCLUDE_FROM_ALL
	ARCHIVE
		DESTINATION vitrio
		COMPONENT vitrio_python_import_library
		EXCLUDE_FROM_ALL
)

install(
	FILES ${VITRIO_PYTHON_PACKAGE_DIR}/_binding.pyi
	DESTINATION vitrio
	COMPONENT vitrio_python
	EXCLUDE_FROM_ALL
)
