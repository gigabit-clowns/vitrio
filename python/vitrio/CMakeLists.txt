# SPDX-License-Identifier: LGPL-2.1-or-later

# The sources are copied beside the extension when the build is configured.
# CMake configures again when one of them changes.
foreach(
	SOURCE
	__init__.py
	_functions.py
	_numerical_type.py
	py.typed
)
	configure_file(${SOURCE} ${VITRIO_PYTHON_PACKAGE_DIR}/${SOURCE} COPYONLY)

	install(
		FILES ${SOURCE}
		DESTINATION vitrio
		COMPONENT vitrio_python
		EXCLUDE_FROM_ALL
	)
endforeach()
