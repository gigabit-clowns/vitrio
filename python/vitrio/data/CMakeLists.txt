# SPDX-License-Identifier: LGPL-2.1-or-later

# The sources are copied beside the extension when the build is configured.
# CMake configures again when one of them changes.
foreach(
	SOURCE
	__init__.py
	_dataset.py
	_frameworks.py
	_loader.py
	_samplers.py
	_sources.py
	_tables.py
)
	configure_file(
		${SOURCE}
		${VITRIO_PYTHON_PACKAGE_DIR}/data/${SOURCE}
		COPYONLY
	)

	install(
		FILES ${SOURCE}
		DESTINATION vitrio/data
		COMPONENT vitrio_python
		EXCLUDE_FROM_ALL
	)
endforeach()
