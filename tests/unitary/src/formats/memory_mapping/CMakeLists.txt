# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_image_file_layout.cpp
		test_image_file_mapping.cpp
		test_image_prefetch_policy.cpp
		test_image_prefetch_schedule.cpp
		test_mapped_image_reader.cpp
		test_mapped_image_writer.cpp
)
