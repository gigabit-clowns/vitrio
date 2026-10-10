# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_tiff_extensions.cpp
		test_tiff_file.cpp
		test_tiff_file_read_format.cpp
		test_tiff_file_write_format.cpp
		test_tiff_page_decoder.cpp
		test_tiff_page_layout.cpp
		test_tiff_reader.cpp
		test_tiff_sample_type.cpp
		test_tiff_signature.cpp
		test_tiff_writer.cpp
)
