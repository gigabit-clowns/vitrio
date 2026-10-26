# SPDX-License-Identifier: LGPL-2.1-or-later

import gzip
import pathlib
import shutil

import numpy as np
import pytest

import vitrio as vio

ASSETS = pathlib.Path(__file__).resolve().parents[1] / 'assets' / 'mrc'

@pytest.fixture(scope='module', autouse=True)
def gzip_mrc():
	try:
		vio.query_descriptor(ASSETS / 'EMD-3197.map.gz')
	except Exception:
		pytest.skip('this build reads no MRC files compressed with gzip')

def __compress(path, compressed_path):
	with open(path, 'rb') as source, gzip.open(compressed_path, 'wb') as target:
		shutil.copyfileobj(source, target)

@pytest.mark.parametrize('name', ['EMD-3197.map', 'EMD-3001.map'])
def test_a_map_of_the_emdb_reads_as_the_file_it_compresses(name):
	plain = np.asarray(vio.read(ASSETS / name))
	compressed = np.asarray(vio.read(ASSETS / (name + '.gz')))
	assert compressed.dtype == plain.dtype
	assert np.array_equal(compressed, plain)

def test_an_image_of_a_compressed_stack_is_read_by_its_index(tmp_path):
	path = tmp_path / 'stack.mrcs'
	source = np.arange(3 * 4 * 6, dtype=np.float32).reshape(3, 4, 6)
	vio.write_stack(source, path)
	compressed_path = tmp_path / 'stack.mrcs.gz'
	__compress(path, compressed_path)

	descriptor = vio.query_descriptor(compressed_path)
	assert descriptor.extents == (3, 4, 6)
	assert descriptor.core_rank == 2
	image = vio.read(vio.ImageLocation(str(compressed_path), 1))
	assert np.array_equal(np.asarray(image), source[1])

def test_a_compressed_volume_is_read_as_the_type_asked_for(tmp_path):
	path = tmp_path / 'volume.mrc'
	source = np.arange(3 * 4 * 6, dtype=np.float32).reshape(3, 4, 6)
	vio.write_single(source, path)
	compressed_path = tmp_path / 'volume.mrc.gz'
	__compress(path, compressed_path)

	volume = np.asarray(vio.read(compressed_path, data_type=np.float64))
	assert volume.dtype == np.float64
	assert np.array_equal(volume, source)
