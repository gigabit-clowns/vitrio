# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-obj
	PRIVATE
		blosc_zarr_bytes_codec.cpp
		crc32c.cpp
		crc32c_zarr_bytes_codec.cpp
		gzip_zarr_bytes_codec.cpp
		zarr_array_layout.cpp
		zarr_bytes_codec.cpp
		zarr_chunk_key_encoding.cpp
		zarr_chunk_layout.cpp
		zarr_codec_pipeline.cpp
		zarr_core_rank.cpp
		zarr_data_type.cpp
		zarr_file_read_format.cpp
		zarr_fill_value.cpp
		zarr_image_reader.cpp
		zarr_json.cpp
		zarr_multiscale.cpp
		zarr_node.cpp
		zarr_read_operation.cpp
		zlib_inflate.cpp
		zlib_zarr_bytes_codec.cpp
		zstd_zarr_bytes_codec.cpp
)
