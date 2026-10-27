# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		gzip_file_stream.cpp
		gzip_identification.cpp
		zlib_deflate.cpp
		zlib_inflate.cpp
		zlib_inflater.cpp
)
