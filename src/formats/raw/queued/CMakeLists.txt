# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		direct_image_read_operation.cpp
		file_read_plan.cpp
		file_read_plan_execution.cpp
		make_image_read_operation.cpp
		queued_image_reader.cpp
		staged_image_read_operation.cpp
)
