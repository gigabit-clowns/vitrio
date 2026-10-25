# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

zarr = pytest.importorskip('zarr')
numcodecs = pytest.importorskip('numcodecs')
codecs = pytest.importorskip('zarr.codecs')


@pytest.fixture(scope='module', autouse=True)
def reads_zarr(tmp_path_factory):
	path = tmp_path_factory.mktemp('probe') / 'probe.zarr'
	array = zarr.create_array(
		store=str(path), shape=(2,), dtype='u1', zarr_format=2)
	array[...] = 1
	try:
		vio.read(path)
	except Exception:
		pytest.skip('this build reads no ZARR arrays')


V2_COMPRESSORS = {
	'none': None,
	'blosc-lz4': numcodecs.Blosc(
		cname='lz4', clevel=5, shuffle=numcodecs.Blosc.SHUFFLE),
	'blosc-zstd-bitshuffle': numcodecs.Blosc(
		cname='zstd', clevel=3, shuffle=numcodecs.Blosc.BITSHUFFLE),
	'zstd': numcodecs.Zstd(level=3),
	'zlib': numcodecs.Zlib(level=6),
	'gzip': numcodecs.GZip(level=6),
}

V3_COMPRESSORS = {
	'none': [],
	'zstd': [codecs.ZstdCodec(level=3)],
	'gzip': [codecs.GzipCodec(level=6)],
	'blosc': [codecs.BloscCodec(cname='lz4', clevel=5)],
	'zstd-crc32c': [codecs.ZstdCodec(level=3), codecs.Crc32cCodec()],
}

DTYPES = [
	'<f4', '>f4', '<f8', '<f2', '<i2', '>i2', '|u1', '|i1', '<u4', '<i8',
	'<c8', '|b1',
]


def __values(shape, dtype):
	count = int(np.prod(shape))
	p = np.arange(count).reshape(shape)
	if np.dtype(dtype).kind == 'b':
		return p % 3 == 0
	if np.dtype(dtype).kind == 'c':
		return (p + 1j * (p % 5)).astype(dtype)
	return (p % 120 - 7 * (np.dtype(dtype).kind != 'u')).astype(dtype)


def __write(path, values, chunks, **options):
	array = zarr.create_array(
		store=str(path), shape=values.shape, chunks=chunks,
		dtype=values.dtype, **options)
	array[...] = values
	return path


def __read(path, **options):
	return np.asarray(vio.read(path, **options))


@pytest.mark.parametrize('compressor', V2_COMPRESSORS, ids=str)
def test_version_2_reads_back_as_zarr_wrote_it(tmp_path, compressor):
	values = __values((9, 10, 11), '<f4')
	path = __write(
		tmp_path / 'a.zarr', values, (4, 4, 4), zarr_format=2,
		compressors=V2_COMPRESSORS[compressor])
	assert np.array_equal(__read(path), values)


@pytest.mark.parametrize('dtype', DTYPES)
def test_every_data_type_reads_back(tmp_path, dtype):
	values = __values((6, 7), dtype)
	path = __write(
		tmp_path / 'a.zarr', values, (4, 3), zarr_format=2,
		compressors=numcodecs.Zstd(level=3))
	read = __read(path)
	assert read.dtype == values.dtype.newbyteorder('=')
	assert np.array_equal(read, values)


@pytest.mark.parametrize('order', ['C', 'F'])
@pytest.mark.parametrize('separator', ['.', '/'])
def test_the_layout_of_version_2_reads_back(tmp_path, order, separator):
	values = __values((5, 6, 7), '<i2')
	path = __write(
		tmp_path / 'a.zarr', values, (2, 4, 3), zarr_format=2, order=order,
		chunk_key_encoding={'name': 'v2', 'separator': separator},
		compressors=numcodecs.Zlib(level=1))
	assert np.array_equal(__read(path), values)


@pytest.mark.parametrize('compressor', V3_COMPRESSORS, ids=str)
@pytest.mark.parametrize('endian', ['little', 'big'])
def test_version_3_reads_back_as_zarr_wrote_it(tmp_path, compressor, endian):
	values = __values((9, 10, 11), '<u2')
	path = __write(
		tmp_path / 'a.zarr', values, (4, 4, 4), zarr_format=3,
		serializer=codecs.BytesCodec(endian=endian),
		compressors=V3_COMPRESSORS[compressor])
	assert np.array_equal(__read(path), values)


def test_a_transposed_array_reads_back(tmp_path):
	values = __values((4, 5, 6), '<f8')
	path = __write(
		tmp_path / 'a.zarr', values, (3, 2, 4), zarr_format=3,
		filters=[codecs.TransposeCodec(order=(1, 2, 0))])
	assert np.array_equal(__read(path), values)


@pytest.mark.parametrize('zarr_format', [2, 3])
def test_chunks_never_written_read_as_the_fill_value(tmp_path, zarr_format):
	path = tmp_path / 'a.zarr'
	array = zarr.create_array(
		store=str(path), shape=(6, 6), chunks=(3, 3), dtype='<f4',
		zarr_format=zarr_format, fill_value=-2.5)
	array[0:3, 3:6] = 1.0

	expected = np.full((6, 6), -2.5, dtype=np.float32)
	expected[0:3, 3:6] = 1.0
	assert np.array_equal(__read(path), expected)


def test_values_are_converted_into_the_type_asked_for(tmp_path):
	values = __values((5, 5), '>i2')
	path = __write(tmp_path / 'a.zarr', values, (2, 2), zarr_format=2)
	read = __read(path, data_type=np.float64)
	assert read.dtype == np.float64
	assert np.array_equal(read, values.astype(np.float64))


def __write_ome(path, levels, scales, zarr_format):
	group = zarr.open_group(str(path), mode='w', zarr_format=zarr_format)
	datasets = []
	for level, (values, scale) in enumerate(zip(levels, scales)):
		array = group.create_array(
			str(level), shape=values.shape, chunks=(1, 4, 4),
			dtype=values.dtype)
		array[...] = values
		datasets.append({
			'path': str(level),
			'coordinateTransformations': [
				{'type': 'scale', 'scale': list(scale)}],
		})
	image = {
		'axes': [{'name': axis, 'type': 'space'} for axis in 'zyx'],
		'datasets': datasets,
	}
	if zarr_format == 2:
		group.attrs['multiscales'] = [dict(image, version='0.4')]
	else:
		group.attrs['ome'] = {'version': '0.5', 'multiscales': [image]}


@pytest.mark.parametrize('zarr_format', [2, 3])
def test_an_ome_zarr_volume_opens_its_finest_level(tmp_path, zarr_format):
	finest = __values((8, 8, 8), '<f4')
	path = tmp_path / 'volume.zarr'
	__write_ome(
		path, [finest, finest[::2, ::2, ::2].copy()],
		[(1, 1, 1), (2, 2, 2)], zarr_format)

	descriptor = vio.query_descriptor(path)
	assert descriptor.extents == (8, 8, 8)
	assert descriptor.core_rank == 3
	assert np.array_equal(__read(path), finest)


@pytest.mark.parametrize('zarr_format', [2, 3])
def test_an_ome_zarr_tilt_series_is_a_stack(tmp_path, zarr_format):
	finest = __values((5, 8, 8), '<u2')
	path = tmp_path / 'tilts.zarr'
	__write_ome(
		path, [finest, finest[:, ::2, ::2].copy()],
		[(1, 1, 1), (1, 2, 2)], zarr_format)

	assert vio.query_descriptor(path).core_rank == 2
	image = np.asarray(vio.read(vio.ImageLocation(path, 3)))
	assert np.array_equal(image, finest[3])

	level = vio.query_descriptor(path / '1')
	assert level.extents == (5, 4, 4)
	assert level.core_rank == 2


def test_a_sharded_array_is_refused(tmp_path):
	values = __values((8, 8), '<u1')
	path = __write(
		tmp_path / 'a.zarr', values, (2, 2), zarr_format=3, shards=(4, 4))
	with pytest.raises(Exception):
		vio.read(path)
