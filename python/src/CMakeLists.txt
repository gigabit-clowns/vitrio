# SPDX-License-Identifier: LGPL-2.1-or-later

target_include_directories(
	vitrio-python
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(
	vitrio-python
	PRIVATE
		caching_image_reader_provider.cpp
		executor_image_loader.cpp
		executor_image_saver.cpp
		file_image_reader_provider.cpp
		file_image_writer_provider.cpp
		image_descriptor.cpp
		image_file_read_format_selector.cpp
		image_file_write_format_selector.cpp
		image_loader.cpp
		image_location.cpp
		image_read.cpp
		image_reader_provider.cpp
		image_saver.cpp
		image_write.cpp
		image_writer_provider.cpp
		index_table.cpp
		index_tuple.cpp
		library_version.cpp
		main.cpp
		utf8_string.cpp
)

add_subdirectory(array)
add_subdirectory(concurrency)
