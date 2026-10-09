# SPDX-License-Identifier: LGPL-2.1-or-later

# Turns on the warnings every target of this project is built with, so that
# the library and the suites are held to one standard rather than two.
function(vitrio_enable_warnings TARGET)
	if(MSVC)
		target_compile_options(
			${TARGET}
			PRIVATE
				/W4
		)
	else()
		target_compile_options(
			${TARGET}
			PRIVATE
				-Wall
				-Wextra
				-Wpedantic
		)
	endif()
endfunction()
