# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		tiff_extensions.cpp
		tiff_file.cpp
		tiff_file_read_format.cpp
		tiff_file_write_format.cpp
		tiff_page_decoder.cpp
		tiff_page_layout.cpp
		tiff_page_regions.cpp
		tiff_reader.cpp
		tiff_region_bounds.cpp
		tiff_sample_type.cpp
		tiff_signature.cpp
		tiff_writer.cpp
)
