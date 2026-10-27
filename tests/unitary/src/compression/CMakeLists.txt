# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_gzip_file_stream.cpp
		test_gzip_identification.cpp
		test_zlib_deflate.cpp
		test_zlib_inflate.cpp
		test_zlib_inflater.cpp
)
