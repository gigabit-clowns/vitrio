# SPDX-License-Identifier: LGPL-2.1-or-later

target_sources(
	vitrio-unitary-tests
	PRIVATE
		test_blosc_zarr_bytes_codec.cpp
		test_crc32c.cpp
		test_crc32c_zarr_bytes_codec.cpp
		test_gzip_zarr_bytes_codec.cpp
		test_zarr_array_layout.cpp
		test_zarr_chunk_key_encoding.cpp
		test_zarr_chunk_layout.cpp
		test_zarr_codec_pipeline.cpp
		test_zarr_core_rank.cpp
		test_zarr_data_type.cpp
		test_zarr_file_read_format.cpp
		test_zarr_fill_value.cpp
		test_zarr_image_reader.cpp
		test_zarr_json.cpp
		test_zarr_multiscale.cpp
		test_zarr_node.cpp
		test_zarr_read_operation.cpp
		test_zlib_inflate.cpp
		test_zlib_zarr_bytes_codec.cpp
		test_zstd_zarr_bytes_codec.cpp
)
