# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_patches_read_around_a_grid_write_the_volume_back(tmp_path):
	# A sliding window over a volume, each tile read with a margin and
	# written back without it: the tiles reach past every edge, and the
	# volume is rebuilt exactly.
	volume = __setup_array((10, 13, 7))
	source = tmp_path / 'source.mrc'
	vio.write_single(volume, source)
	target = tmp_path / 'target.mrc'
	writers = vio.writer_provider()
	writers.declare(
		target,
		vio.ImageDescriptor(volume.shape, 3, vio.NumericalType.float32)
	)
	saver = vio.saver(writers)
	loader = vio.loader()

	tile, margin = 4, 2
	centres = np.stack(np.meshgrid(
		*(np.arange(tile // 2, extent + tile // 2, tile)
			for extent in volume.shape),
		indexing='ij'
	), axis=-1).reshape(-1, 3)
	crops = np.zeros((len(centres),) + (tile + 2 * margin,) * 3, np.float32)
	vio.read_patches_async(
		loader, crops, vio.ImageLocation(source), centres
	).get()
	kept = np.ascontiguousarray(crops[:, margin:-margin, margin:-margin,
		margin:-margin])
	vio.write_patches_async(
		saver, kept, vio.ImageLocation(target), centres
	).get()
	writers.close(target)

	assert np.array_equal(np.asarray(vio.read(target)), volume)

def test_patches_are_written_into_the_image_of_a_stack_named(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(
		path, vio.ImageDescriptor((3, 4, 6), 2, vio.NumericalType.float32)
	)
	saver = vio.saver(writers)
	stack = __setup_array((3, 4, 6))
	vio.write_batch_async(
		saver, stack, [vio.ImageLocation(path, i) for i in range(3)]
	).get()
	patch = np.full((1, 2, 2), -1, np.float32)
	vio.write_patches_async(
		saver, patch, vio.ImageLocation(path, 1), [(1, 3)]
	).get()
	writers.close(path)

	expected = stack.copy()
	expected[1, 0:2, 2:4] = -1
	assert np.array_equal(np.asarray(vio.read(path)), expected)

def test_an_empty_batch_of_patches_is_already_done(tmp_path):
	path = tmp_path / 'image.mrc'
	writers = vio.writer_provider()
	writers.declare(
		path, vio.ImageDescriptor((8, 8), 2, vio.NumericalType.float32)
	)
	completion = vio.write_patches_async(
		vio.saver(writers), np.zeros((0, 4, 4), np.float32),
		vio.ImageLocation(path), []
	)
	assert completion.is_ready

def test_a_source_of_the_wrong_batch_size_is_refused(tmp_path):
	path = tmp_path / 'image.mrc'
	writers = vio.writer_provider()
	writers.declare(
		path, vio.ImageDescriptor((8, 8), 2, vio.NumericalType.float32)
	)
	with pytest.raises(ValueError):
		vio.write_patches_async(
			vio.saver(writers), np.zeros((3, 4, 4), np.float32),
			vio.ImageLocation(path), [(2, 2), (6, 6)]
		)

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)
