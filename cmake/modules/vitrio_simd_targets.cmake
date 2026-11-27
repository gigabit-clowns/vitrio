# SPDX-License-Identifier: LGPL-2.1-or-later

# The instruction sets the vector loops may be compiled for beyond the one the
# compiler flags allow, by Highway's names without HWY_, and how a choice
# among them becomes what Highway is told to leave out.
#
#   vitrio_simd_default_targets(<variable>)
#   vitrio_simd_disabled_targets(<variable> [<target>...])
#
# Each architecture has a table of its targets, from the worst to the best,
# every one of which Highway names since its release 1.2.0. The lowest
# target of an architecture, the one of the baseline, is not in it: Highway
# must keep the target of the compiler flags among the ones it compiles.
#
# Highway gives a better target a lower bit, and so writes (HWY_X - 1) for
# every target better than X. The targets left out are the ones of the table
# that were not asked for, and every target better than the best that was,
# through that idiom: so a target a later release of Highway adds stays out
# too, without being named here.
#
# On x86 the default is AVX2, whose F16C converts half precision values, and
# AVX3. On aarch64 it is nothing: NEON, the baseline, converts them already.

set(VITRIO_SIMD_TARGETS_X86 SSSE3 SSE4 AVX2 AVX3 AVX3_DL AVX3_ZEN4 AVX3_SPR)
set(VITRIO_SIMD_TARGETS_AARCH64 NEON_BF16 SVE SVE2 SVE_256 SVE2_128)

function(vitrio_simd_architecture_table variable)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x64)$")
		set(${variable} ${VITRIO_SIMD_TARGETS_X86} PARENT_SCOPE)
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
		set(${variable} ${VITRIO_SIMD_TARGETS_AARCH64} PARENT_SCOPE)
	else()
		set(${variable} "" PARENT_SCOPE)
	endif()
endfunction()

function(vitrio_simd_default_targets variable)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x64)$")
		set(${variable} AVX2 AVX3 PARENT_SCOPE)
	else()
		set(${variable} "" PARENT_SCOPE)
	endif()
endfunction()

function(vitrio_simd_disabled_targets variable)
	vitrio_simd_architecture_table(table)

	set(disabled "")
	set(best "")
	foreach(target IN LISTS ARGN)
		if(NOT target IN_LIST table)
			string(REPLACE ";" ", " known "${table}")
			message(
				FATAL_ERROR
				"VITRIO_SIMD_TARGETS names ${target}, which is not a target "
				"of this architecture. The targets are: ${known}"
			)
		endif()
	endforeach()
	foreach(target IN LISTS table)
		if(target IN_LIST ARGN)
			set(best ${target})
		else()
			list(APPEND disabled "HWY_${target}")
		endif()
	endforeach()

	if(NOT table)
		set(${variable} 0 PARENT_SCOPE)
		return()
	endif()
	if(NOT best)
		list(GET table 0 best)
	endif()
	list(APPEND disabled "(HWY_${best} - 1)")
	string(REPLACE ";" "|" expression "${disabled}")
	set(${variable} "(${expression})" PARENT_SCOPE)
endfunction()
