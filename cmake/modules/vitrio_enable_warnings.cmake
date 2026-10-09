# SPDX-License-Identifier: LGPL-2.1-or-later

# Sets the warnings every target of this project is built with, so that the
# library and the suites are held to one standard rather than two.
function(vitrio_enable_warnings TARGET)
	if(MSVC)
		target_compile_options(
			${TARGET}
			PRIVATE
				/W4
		)

		# std::complex is specified for float, double and long double only,
		# and the standard library of MSVC says so wherever it is used with
		# anything else. A complex number of half precision is one: every
		# standard library lays it out as its two components, which is all
		# that is relied on.
		target_compile_definitions(
			${TARGET}
			PRIVATE
				_SILENCE_NONFLOATING_COMPLEX_DEPRECATION_WARNING
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
