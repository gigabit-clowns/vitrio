# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_direct_image_read_operation.cpp
		test_file_read_plan.cpp
		test_file_read_plan_execution.cpp
		test_make_image_read_operation.cpp
		test_queued_image_reader.cpp
		test_staged_image_read_operation.cpp
)
