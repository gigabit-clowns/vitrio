# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_direct_image_read_operation.cpp
		test_direct_image_write_operation.cpp
		test_file_read_plan.cpp
		test_file_read_plan_execution.cpp
		test_file_sync_operation.cpp
		test_file_write_plan.cpp
		test_make_image_read_operation.cpp
		test_make_image_write_operation.cpp
		test_queued_image_reader.cpp
		test_queued_image_writer.cpp
		test_staged_image_read_operation.cpp
		test_staged_image_write_operation.cpp
		test_writable_image_file.cpp
)
