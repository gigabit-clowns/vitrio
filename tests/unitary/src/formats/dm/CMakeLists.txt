# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_dm_data_type.cpp
		test_dm_encoding.cpp
		test_dm_extensions.cpp
		test_dm_file_read_format.cpp
		test_dm_geometry.cpp
		test_dm_header.cpp
		test_dm_image_location.cpp
		test_dm_tag.cpp
		test_dm_tag_parser.cpp
		test_dm_tag_values.cpp
)
