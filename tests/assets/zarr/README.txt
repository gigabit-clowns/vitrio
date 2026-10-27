ZARR test assets

These stores are here so that the ZARR reader is tested against bytes this
project did not write through its own code. zarr-python 3.4.1, with
numcodecs 0.17.0 and NumPy 2.5.3, wrote every one of them, through
make_zarr_assets.py beside them:

  python make_zarr_assets.py tests/assets/zarr

Running it again writes the same bytes, but for the chunks compressed with
gzip, whose header holds the time they were written.

Each store is a directory, as ZARR stores an array: its metadata in .zarray
and .zattrs, or zarr.json, and a file for each chunk. zarr-python writes no
file for a chunk that holds nothing but the fill value, and that is read as
the fill value, as a chunk that is not stored is.


The values

p counts the values of an array from zero in C order, the last axis changing
fastest, whatever order the chunks store them in. Each array holds a scale of
p plus an offset, cast to its data type, as each entry says.


v2_float32_blosc_lz4.zarr
Version 2, float32 little endian, 5 by 6 by 7 values of 0.25 p - 4 in chunks
of 2 by 4 by 3, which the edges of the array cut. Blosc with lz4 at level 5
and the bytes shuffled, its chunks named with "/" between their positions:
as the CryoET Data Portal stores a tomogram.

v2_int16_big_endian_f_zlib.zarr
Version 2, int16 big endian in the order F, 6 by 10 values of 500 - 3 p in
chunks of 4 by 4. zlib at level 6, its chunks named with "." between their
positions.

v2_uint8_gzip_missing_chunk.zarr
Version 2, uint8, 4 by 6 values of p + 20 in chunks of 2 by 3, gzip at level
6. The chunk 1.0, rows 2 and 3 of columns 0 to 2, was removed after it was
written, so those values read as the fill value, 7.

v2_float32_zstd_nan_fill.zarr
Version 2, float32, 4 by 4 values in chunks of 2 by 2, zstd at level 3. Only
rows 0 and 1 were written, as p + 1 counted over those two rows. The rest
read as the fill value, NaN.

v2_bool.zarr
Version 2, booleans, 3 by 5 values, true where p is a multiple of 3, in
chunks of 2 by 2, uncompressed. The chunks 1.0 and 1.2 hold false alone,
and are not stored.

v3_float32_transpose_zstd_crc32c.zarr
Version 3, float32, 3 by 4 by 5 values of 0.5 p + 1 in chunks of 2 by 3 by 2.
Its codecs are a transpose of the order 2, 0, 1, the bytes little endian,
zstd at level 3 and a CRC-32C checksum.

v3_uint16_big_endian_gzip.zarr
Version 3, uint16, 7 by 9 values of 7 p + 1000 in chunks of 4 by 4, the bytes
big endian and gzip at level 6.

v3_uint16_sharded.zarr
Version 3, uint16, 6 by 10 values of 3 p + 5, in shards of 4 by 4 of chunks of
2 by 2, the index at the end of each shard and checked with crc32c, each chunk
zstd at level 3. The values of rows 0 and 1 of columns 0 and 1, and those of
rows 4 and 5 of columns 8 and 9, are 0, the fill value: the first shard does
not store the chunk at the origin, and the shard of the last rows and columns
is not stored at all.

v3_float32_sharded_index_start.zarr
Version 3, float32, 4 by 6 values of 0.5 p - 2, one shard of chunks of 2 by 3,
its index at the start and not checked. Each chunk is transposed, its bytes
big endian, and gzip at level 6, so this shard too differs between two runs.

ome_v04_tomogram.zarr
A group of version 2 that is an image of OME-Zarr 0.4: axes z, y and x, all
of space, in angstrom. Its levels 0, 1 and 2 are 8, 4 and 2 values a side,
float32 in chunks of 4 a side, blosc with zstd. Level 0 holds p, and each
level after it every second value of the one before along every axis, so
its scale doubles along every axis: a volume.

ome_v05_tilt_series.zarr
A group of version 3 that is an image of OME-Zarr 0.5, its axes z, y and x
all of space. Level 0 is 5 by 8 by 8 values of p, uint16, in chunks of 1 by 4
by 4, zstd. Level 1 holds every second value of it along y and x, and all
five along z, whose scale stays 1: a stack of five images, as the CryoET
Data Portal stores a tilt series. Its arrays name their dimensions z, y and
x.
