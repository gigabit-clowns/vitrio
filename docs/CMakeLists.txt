# SPDX-License-Identifier: LGPL-2.1-or-later

# Each reference has a directory of its own. The Python one needs the
# package, so it is only there when that is built.
add_subdirectory(doxygen)

if(VITRIO_BUILD_PYTHON)
	add_subdirectory(sphinx)
endif()
