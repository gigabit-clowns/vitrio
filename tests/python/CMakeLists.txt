# SPDX-License-Identifier: LGPL-2.1-or-later

# The suite imports the package from the build tree. pytest is kept from
# writing its cache and Python its bytecode, both of which would land in the
# source tree.
add_test(
	NAME vitrio-python-tests
	COMMAND
		${Python_EXECUTABLE} -m pytest
		-p no:cacheprovider
		${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(
	vitrio-python-tests
	PROPERTIES
		ENVIRONMENT
			"PYTHONPATH=${PROJECT_BINARY_DIR}/python;PYTHONDONTWRITEBYTECODE=1"
		LABELS python
		TIMEOUT 3600
)
