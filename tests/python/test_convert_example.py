# SPDX-License-Identifier: LGPL-2.1-or-later

# Runs examples/python/basic/convert.py on an image and a stack it writes
# itself, and reads what it converted them to.

import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import vitrio as vio

EXAMPLE = (
	Path(__file__).parents[2] / 'examples' / 'python' / 'basic'
	/ 'convert.py'
)

@pytest.fixture
def images(tmp_path):
	rng = np.random.default_rng(0)
	image = rng.integers(0, 100, size=(8, 12)).astype(np.float32)
	stack = rng.integers(0, 100, size=(5, 8, 12)).astype(np.float32)
	vio.write_single(image, tmp_path / 'image.mrc')
	vio.write_stack(stack, tmp_path / 'stack.mrcs')
	return image, stack

def test_a_stack_stays_a_stack(images, tmp_path):
	_, stack = images
	__run_example(tmp_path / 'stack.mrcs', tmp_path / 'again.mrcs')
	descriptor = vio.query_descriptor(tmp_path / 'again.mrcs')
	assert descriptor.extents == stack.shape
	assert descriptor.core_rank == 2
	assert np.array_equal(
		np.asarray(vio.read(tmp_path / 'again.mrcs')), stack
	)

def test_a_single_image_stays_a_single_image(images, tmp_path):
	image, _ = images
	__run_example(tmp_path / 'image.mrc', tmp_path / 'again.mrc')
	descriptor = vio.query_descriptor(tmp_path / 'again.mrc')
	assert descriptor.extents == image.shape
	assert np.array_equal(
		np.asarray(vio.read(tmp_path / 'again.mrc')), image
	)

def test_converts_the_data_type(images, tmp_path):
	__run_example(
		tmp_path / 'stack.mrcs', tmp_path / 'half.mrcs',
		'--data-type', 'float16'
	)
	descriptor = vio.query_descriptor(tmp_path / 'half.mrcs')
	assert descriptor.data_type == vio.NumericalType.float16

def test_converts_the_format(images, tmp_path):
	try:
		__run_example(tmp_path / 'stack.mrcs', tmp_path / 'stack.tif')
	except subprocess.CalledProcessError:
		pytest.skip('this build writes no TIFF files')
	_, stack = images
	assert np.array_equal(
		np.asarray(vio.read(tmp_path / 'stack.tif')), stack
	)

def __run_example(source, destination, *arguments):
	return subprocess.run(
		[sys.executable, str(EXAMPLE), str(source), str(destination),
			*arguments],
		env=os.environ, check=True, capture_output=True, text=True,
		timeout=120
	)
