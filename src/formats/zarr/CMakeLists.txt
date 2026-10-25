# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		blosc_zarr_bytes_codec.cpp
		crc32c.cpp
		crc32c_zarr_bytes_codec.cpp
		gzip_zarr_bytes_codec.cpp
		zarr_bytes_codec.cpp
		zarr_codec_pipeline.cpp
		zlib_inflate.cpp
		zlib_zarr_bytes_codec.cpp
		zstd_zarr_bytes_codec.cpp
)
