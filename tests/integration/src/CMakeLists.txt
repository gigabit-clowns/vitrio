# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-integration-tests
	PRIVATE
		test_library_version.cpp
		test_mrc_real_files.cpp
		test_mrc_round_trip.cpp
		test_public_constants.cpp
)

if(VITRIO_ENABLE_TIFF)
	target_sources(
		vitrio-integration-tests
		PRIVATE
			test_tiff_real_files.cpp
			test_tiff_round_trip.cpp
	)
endif()
