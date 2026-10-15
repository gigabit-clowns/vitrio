# SPDX-License-Identifier: LGPL-2.1-or-later

# The reference of the Python package is built from the docstrings of the
# package that is imported, which is the one under `python/` of the build
# tree, so the extension and its stubs come first. A warning of Sphinx fails
# it, as one of Doxygen fails the other.
execute_process(
	COMMAND ${Python_EXECUTABLE} -c "import sphinx, furo, numpy"
	RESULT_VARIABLE VITRIO_SPHINX_RESULT
	OUTPUT_QUIET
	ERROR_QUIET
)
if(VITRIO_SPHINX_RESULT)
	message(
		FATAL_ERROR
		"VITRIO_BUILD_DOCS with VITRIO_BUILD_PYTHON needs Sphinx, Furo and numpy "
		"in the interpreter ${Python_EXECUTABLE}: pip install --group docs"
	)
endif()

add_custom_target(
	vitrio-python-docs
	COMMAND
		${CMAKE_COMMAND} -E env
		PYTHONPATH=${PROJECT_BINARY_DIR}/python
		${Python_EXECUTABLE} -m sphinx
		-W --keep-going -q -b html
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}/html
	DEPENDS vitrio-python-stubs
	COMMENT "Generating the documentation of the Python package"
	VERBATIM
)
