# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		image_file_layout.cpp
		image_file_mapping.cpp
		image_prefetch_policy.cpp
		image_prefetch_schedule.cpp
		mapped_image_reader.cpp
		mapped_image_writer.cpp
)
