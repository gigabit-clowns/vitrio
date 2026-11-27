# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_file_range.cpp
		test_image_read_strategy.cpp
		test_image_region_runs.cpp
		test_image_region_split.cpp
		test_image_transfer_policy.cpp
		test_image_transfer_schedule.cpp
)
