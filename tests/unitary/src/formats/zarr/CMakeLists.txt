# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_blosc_zarr_bytes_codec.cpp
		test_crc32c.cpp
		test_crc32c_zarr_bytes_codec.cpp
		test_gzip_zarr_bytes_codec.cpp
		test_zarr_codec_pipeline.cpp
		test_zlib_inflate.cpp
		test_zlib_zarr_bytes_codec.cpp
		test_zstd_zarr_bytes_codec.cpp
)
