# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_crops_patches_given_as_a_list_of_centres(__written_image):
	loader = vio.loader()
	destination = np.zeros((2, 8, 8), dtype=np.float32)
	vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image),
		[(10, 10), (20, 30)]
	).get()
	image = __setup_array((32, 48))
	assert np.array_equal(destination[0], image[6:14, 6:14])
	assert np.array_equal(destination[1], image[16:24, 26:34])

def test_crops_patches_given_as_an_index_table(__written_image):
	loader = vio.loader()
	centres = vio.IndexTable(2)
	centres.add((10, 10))
	centres.add((20, 30))
	destination = np.zeros((2, 8, 8), dtype=np.float32)
	vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image), centres
	).get()
	image = __setup_array((32, 48))
	assert np.array_equal(destination[1], image[16:24, 26:34])

def test_crops_patches_given_as_an_array_of_centres(__written_image):
	loader = vio.loader()
	centres = np.array([[10, 10], [20, 30]])
	destination = np.zeros((2, 8, 8), dtype=np.float32)
	vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image), centres
	).get()
	image = __setup_array((32, 48))
	assert np.array_equal(destination[1], image[16:24, 26:34])

@pytest.mark.parametrize(
	"centre",
	[
		pytest.param((0, 0), id="Before the origin"),
		pytest.param((31, 47), id="Past the far corner"),
		pytest.param((100, 100), id="Outside the image"),
	]
)
def test_a_patch_over_a_border_is_clipped_rather_than_refused(
	centre, __written_image
):
	loader = vio.loader()
	destination = np.zeros((1, 8, 8), dtype=np.float32)
	completion = vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image), [centre]
	)
	completion.get()
	assert completion.is_ready

def test_what_a_clipped_patch_does_not_reach_is_left_as_it_was(
	__written_image
):
	loader = vio.loader()
	destination = np.full((1, 8, 8), -1, dtype=np.float32)
	vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image), [(0, 0)]
	).get()
	image = __setup_array((32, 48))
	assert np.all(destination[0, :4, :] == -1)
	assert np.all(destination[0, :, :4] == -1)
	assert np.array_equal(destination[0, 4:, 4:], image[:4, :4])

def test_crops_out_of_one_image_of_a_stack(__written_stack):
	loader = vio.loader()
	destination = np.zeros((1, 4, 4), dtype=np.float32)
	vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_stack, 1),
		[(2, 3)]
	).get()
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(destination[0], stack[1, 0:4, 1:5])

def test_an_empty_batch_is_already_done(__written_image):
	loader = vio.loader()
	destination = np.zeros((0, 8, 8), dtype=np.float32)
	completion = vio.read_patches_async(
		loader, destination, vio.ImageLocation(__written_image), []
	)
	assert completion.is_ready

def test_a_destination_of_the_wrong_batch_size_is_refused(__written_image):
	loader = vio.loader()
	destination = np.zeros((3, 8, 8), dtype=np.float32)
	location = vio.ImageLocation(__written_image)
	with pytest.raises(ValueError):
		vio.read_patches_async(
			loader, destination, location, [(10, 10), (20, 30)]
		)

def test_a_centre_of_another_rank_is_refused(__written_image):
	loader = vio.loader()
	destination = np.zeros((1, 8, 8), dtype=np.float32)
	location = vio.ImageLocation(__written_image)
	with pytest.raises(ValueError):
		vio.read_patches_async(
			loader, destination, location, [(10, 10, 10)]
		)

def test_an_index_table_of_another_rank_is_refused(__written_image):
	loader = vio.loader()
	centres = vio.IndexTable(3)
	centres.add((10, 10, 10))
	destination = np.zeros((1, 8, 8), dtype=np.float32)
	location = vio.ImageLocation(__written_image)
	with pytest.raises(ValueError):
		vio.read_patches_async(loader, destination, location, centres)

def test_an_array_of_centres_of_another_rank_is_refused(__written_image):
	loader = vio.loader()
	destination = np.zeros((1, 8, 8), dtype=np.float32)
	location = vio.ImageLocation(__written_image)
	centres = np.array([[10, 10, 10]])
	with pytest.raises(ValueError):
		vio.read_patches_async(loader, destination, location, centres)

def test_centres_that_are_neither_table_array_nor_sequence_are_refused(
	__written_image
):
	loader = vio.loader()
	destination = np.zeros((1, 8, 8), dtype=np.float32)
	location = vio.ImageLocation(__written_image)
	with pytest.raises(TypeError):
		vio.read_patches_async(loader, destination, location, 10)

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

@pytest.fixture
def __written_image(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((32, 48)), path)
	return path

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	return path
