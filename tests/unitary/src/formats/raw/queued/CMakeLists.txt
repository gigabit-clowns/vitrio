# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_file_read_plan.cpp
		test_file_read_plan_execution.cpp
		test_image_queued_read.cpp
		test_queued_image_reader.cpp
)
