# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		direct_image_read_operation.cpp
		direct_image_write_operation.cpp
		file_read_plan.cpp
		file_read_plan_execution.cpp
		file_sync_operation.cpp
		file_write_plan.cpp
		make_image_read_operation.cpp
		make_image_write_operation.cpp
		queued_image_reader.cpp
		queued_image_writer.cpp
		staged_image_read_operation.cpp
		staged_image_write_operation.cpp
		writable_image_file.cpp
)
