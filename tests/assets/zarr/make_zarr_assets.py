# SPDX-License-Identifier: LGPL-2.1-or-later

"""Write the ZARR stores of tests/assets/zarr, as README.txt describes.

Run as: python make_zarr_assets.py <directory>

It needs zarr-python and numcodecs, at the versions README.txt names, and
replaces each store it writes.
"""

import json
import os
import shutil
import sys

import numcodecs
import numpy as np
import zarr
from zarr.codecs import (
	BloscCodec, BytesCodec, Crc32cCodec, GzipCodec, ShardingCodec,
	TransposeCodec, ZstdCodec,
)


def counting(shape, dtype, scale=1, offset=0):
	"""p, the position of each value in C order from zero, as README.txt has
	it, times scale plus offset."""
	p = np.arange(int(np.prod(shape)), dtype=np.float64).reshape(shape)
	return (p * scale + offset).astype(dtype)


def remove(path):
	if os.path.isdir(path):
		shutil.rmtree(path)


def write_v2(root, name, values, chunks, **options):
	path = os.path.join(root, name)
	remove(path)
	array = zarr.create_array(
		store=path, shape=values.shape, chunks=chunks, dtype=values.dtype,
		zarr_format=2, **options)
	array[...] = values
	return path


def write_v3(root, name, values, chunks, **options):
	path = os.path.join(root, name)
	remove(path)
	array = zarr.create_array(
		store=path, shape=values.shape, chunks=chunks, dtype=values.dtype,
		zarr_format=3, **options)
	array[...] = values
	return path


def portal_tomogram(root):
	"""Version 2, float32, blosc with lz4 and the bytes shuffled, chunks in
	directories: as the CryoET Data Portal stores a tomogram."""
	write_v2(
		root, 'v2_float32_blosc_lz4.zarr',
		counting((5, 6, 7), np.float32, 0.25, -4), (2, 4, 3),
		compressors=numcodecs.Blosc(
			cname='lz4', clevel=5, shuffle=numcodecs.Blosc.SHUFFLE),
		chunk_key_encoding={'name': 'v2', 'separator': '/'},
		fill_value=0.0)


def big_endian_fortran(root):
	"""Version 2, int16 big endian in the order F, zlib, chunks named with
	dots."""
	write_v2(
		root, 'v2_int16_big_endian_f_zlib.zarr',
		counting((6, 10), '>i2', -3, 500), (4, 4),
		compressors=numcodecs.Zlib(level=6), order='F',
		chunk_key_encoding={'name': 'v2', 'separator': '.'},
		fill_value=0)


def missing_chunk(root):
	"""Version 2, uint8, gzip, one chunk removed: it reads as the fill value,
	7."""
	path = write_v2(
		root, 'v2_uint8_gzip_missing_chunk.zarr',
		counting((4, 6), np.uint8, 1, 20), (2, 3),
		compressors=numcodecs.GZip(level=6),
		chunk_key_encoding={'name': 'v2', 'separator': '.'},
		fill_value=7)
	os.remove(os.path.join(path, '1.0'))


def nan_fill(root):
	"""Version 2, float32, zstd, of which only the first row of chunks was
	written: the rest reads as NaN."""
	path = os.path.join(root, 'v2_float32_zstd_nan_fill.zarr')
	remove(path)
	array = zarr.create_array(
		store=path, shape=(4, 4), chunks=(2, 2), dtype=np.float32,
		zarr_format=2, compressors=numcodecs.Zstd(level=3),
		fill_value=float('nan'))
	array[0:2, :] = counting((2, 4), np.float32, 1, 1)


def booleans(root):
	"""Version 2, booleans, uncompressed."""
	values = (np.arange(15).reshape(3, 5) % 3) == 0
	write_v2(
		root, 'v2_bool.zarr', values, (2, 2), compressors=None,
		fill_value=False)


def transposed(root):
	"""Version 3, float32, transposed, zstd and a checksum."""
	write_v3(
		root, 'v3_float32_transpose_zstd_crc32c.zarr',
		counting((3, 4, 5), np.float32, 0.5, 1), (2, 3, 2),
		filters=[TransposeCodec(order=(2, 0, 1))],
		serializer=BytesCodec(endian='little'),
		compressors=[ZstdCodec(level=3), Crc32cCodec()],
		fill_value=0)


def big_endian_gzip(root):
	"""Version 3, uint16 big endian, gzip."""
	write_v3(
		root, 'v3_uint16_big_endian_gzip.zarr',
		counting((7, 9), np.uint16, 7, 1000), (4, 4),
		serializer=BytesCodec(endian='big'),
		compressors=[GzipCodec(level=6)],
		fill_value=0)


def sharded(root):
	"""Version 3, uint16, in shards of 4 by 4 of chunks of 2 by 2, its index
	at the end and checked. The chunk at the origin is all zeros, the fill
	value, and its shard does not store it; nor is the shard of the last rows
	and columns stored, all zeros too."""
	values = counting((6, 10), np.uint16, 3, 5)
	values[0:2, 0:2] = 0
	values[4:6, 8:10] = 0
	write_v3(
		root, 'v3_uint16_sharded.zarr', values, (2, 2), shards=(4, 4),
		compressors=[ZstdCodec(level=3)], fill_value=0)


def sharded_index_first(root):
	"""Version 3, float32, one shard of 4 by 6 of chunks of 2 by 3, its index
	at the start and unchecked; each chunk transposed, big endian and gzip."""
	path = os.path.join(root, 'v3_float32_sharded_index_start.zarr')
	remove(path)
	values = counting((4, 6), np.float32, 0.5, -2)
	array = zarr.create_array(
		store=path, shape=values.shape, chunks=values.shape,
		dtype=np.float32, zarr_format=3, fill_value=0, compressors=None,
		serializer=ShardingCodec(
			chunk_shape=(2, 3),
			codecs=[
				TransposeCodec(order=(1, 0)), BytesCodec(endian='big'),
				GzipCodec(level=6)],
			index_codecs=[BytesCodec(endian='little')],
			index_location='start'))
	array[...] = values


def ome_tomogram(root):
	"""An image of OME-Zarr 0.4, over version 2: three levels of a volume,
	each half the one before along every axis."""
	path = os.path.join(root, 'ome_v04_tomogram.zarr')
	remove(path)
	group = zarr.open_group(path, mode='w', zarr_format=2)
	finest = counting((8, 8, 8), np.float32, 1, 0)
	datasets = []
	for level in range(3):
		step = 2 ** level
		values = np.ascontiguousarray(finest[::step, ::step, ::step])
		array = group.create_array(
			str(level), shape=values.shape, chunks=(4, 4, 4),
			dtype=np.float32,
			compressors=numcodecs.Blosc(
				cname='zstd', clevel=5, shuffle=numcodecs.Blosc.SHUFFLE),
			chunk_key_encoding={'name': 'v2', 'separator': '/'},
			fill_value=0.0)
		array[...] = values
		datasets.append({
			'path': str(level),
			'coordinateTransformations': [
				{'type': 'scale', 'scale': [10.0 * step] * 3}],
		})
	group.attrs['multiscales'] = [{
		'version': '0.4',
		'axes': [
			{'name': axis, 'type': 'space', 'unit': 'angstrom'}
			for axis in 'zyx'],
		'datasets': datasets,
	}]


def ome_tilt_series(root):
	"""An image of OME-Zarr 0.5, over version 3: a stack of images stored as
	three axes of space, whose second level halves the last two only, as the
	CryoET Data Portal stores a tilt series."""
	path = os.path.join(root, 'ome_v05_tilt_series.zarr')
	remove(path)
	group = zarr.open_group(path, mode='w', zarr_format=3)
	finest = counting((5, 8, 8), np.uint16, 1, 0)
	datasets = []
	for level in range(2):
		step = 2 ** level
		values = np.ascontiguousarray(finest[:, ::step, ::step])
		array = group.create_array(
			str(level), shape=values.shape, chunks=(1, 4, 4),
			dtype=np.uint16, dimension_names=['z', 'y', 'x'],
			compressors=[ZstdCodec(level=3)], fill_value=0)
		array[...] = values
		datasets.append({
			'path': str(level),
			'coordinateTransformations': [
				{'type': 'scale', 'scale': [1.0, 2.0 * step, 2.0 * step]}],
		})
	group.attrs['ome'] = {
		'version': '0.5',
		'multiscales': [{
			'axes': [
				{'name': axis, 'type': 'space', 'unit': 'angstrom'}
				for axis in 'zyx'],
			'datasets': datasets,
		}],
	}


def main():
	if len(sys.argv) != 2:
		sys.exit(__doc__)

	root = sys.argv[1]
	for write in (
		portal_tomogram, big_endian_fortran, missing_chunk, nan_fill,
		booleans, transposed, big_endian_gzip, sharded, sharded_index_first,
		ome_tomogram, ome_tilt_series,
	):
		write(root)


if __name__ == '__main__':
	main()
