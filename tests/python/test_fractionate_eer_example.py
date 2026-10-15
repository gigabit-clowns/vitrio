# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs examples/python/advanced/fractionate_eer.py on the EER files of
# tests/assets/eer, and compares what it writes with the movie the package
# reads the same file as.

import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

EXAMPLE = (
	Path(__file__).parents[2] / 'examples' / 'python' / 'advanced'
	/ 'fractionate_eer.py'
)
ASSETS = Path(__file__).parents[1] / 'assets' / 'eer'
MOVIE = ASSETS / 'movie_rle7.eer'    # 4 frames of 8 by 6 pixels

@pytest.fixture(scope='module', autouse=True)
def eer():
	try:
		vio.detector_event_timeline_reader_provider().acquire(MOVIE)
	except Exception:
		pytest.skip('this build reads no EER files')

def test_writes_the_fractions_of_a_movie_as_a_stack(tmp_path):
	__run_example(tmp_path, '-n', '2')
	descriptor = vio.query_descriptor(tmp_path / 'movie_rle7.mrcs')
	assert descriptor.extents == (2, 6, 8)
	assert descriptor.core_rank == 2

def test_the_fractions_are_those_the_provider_reads(tmp_path):
	__run_example(tmp_path, '-n', '3', '--binning', '2')
	expected = np.asarray(vio.read(
		MOVIE, vio.detector_event_image_reader_provider(3, binning=2),
		data_type='float32'
	))
	stored = np.asarray(vio.read(tmp_path / 'movie_rle7.mrcs'))
	assert stored.shape == (3, 3, 4)
	assert np.array_equal(stored, expected)

def test_stores_the_requested_data_type(tmp_path):
	__run_example(tmp_path, '-n', '2', '--data-type', 'uint16')
	descriptor = vio.query_descriptor(tmp_path / 'movie_rle7.mrcs')
	assert descriptor.data_type == vio.NumericalType.uint16

def test_writes_tiff_when_asked_to(tmp_path):
	try:
		__run_example(tmp_path, '-n', '2', '--format', 'tiff')
	except subprocess.CalledProcessError:
		pytest.skip('this build writes no TIFF files')
	descriptor = vio.query_descriptor(tmp_path / 'movie_rle7.tif')
	assert descriptor.extents == (2, 6, 8)

def test_a_binning_that_does_not_divide_the_movie_is_refused(tmp_path):
	with pytest.raises(subprocess.CalledProcessError):
		__run_example(tmp_path, '-n', '2', '--binning', '4')

def __run_example(output, *arguments):
	return subprocess.run(
		[sys.executable, str(EXAMPLE), str(MOVIE), '-o', str(output),
			*arguments],
		env=os.environ, check=True, capture_output=True, text=True,
		timeout=120
	)
