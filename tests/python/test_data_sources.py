# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_an_image_source_has_the_shape_of_one_image(__written_stack):
	source = vio.data.ImageSource(__locations(__written_stack, [0, 1]))
	assert len(source) == 2
	assert source.shape == (4, 6)
	assert source.dtype == np.float32

def test_an_image_source_reads_the_rows_asked_for(__written_stack):
	source = vio.data.ImageSource(__locations(__written_stack, [0, 1, 2]))
	out = np.empty((2, 4, 6), np.float32)
	__wait(source.read_async(vio.loader(), np.array([2, 0]), out))
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(out, stack[[2, 0]])

def test_an_image_source_reads_as_the_type_asked_for(__written_stack):
	source = vio.data.ImageSource(
		__locations(__written_stack, [1]), data_type='float64'
	)
	out = np.empty((1, 4, 6), source.dtype)
	__wait(source.read_async(vio.loader(), np.array([0]), out))
	assert source.dtype == np.float64
	assert np.array_equal(out[0], __setup_array((3, 4, 6))[1])

def test_an_image_source_takes_keys_of_whole_files(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((4, 6)), path)
	source = vio.data.ImageSource([str(path), path])
	out = np.empty((2, 4, 6), np.float32)
	__wait(source.read_async(vio.loader(), np.array([0, 1]), out))
	assert np.array_equal(out[1], __setup_array((4, 6)))

def test_an_image_source_of_rows_of_locations_stacks_them(__written_stack):
	rows = [
		__locations(__written_stack, [0, 2]),
		__locations(__written_stack, [1, 0]),
	]
	source = vio.data.ImageSource(rows)
	assert source.shape == (2, 4, 6)
	out = np.empty((2, 2, 4, 6), np.float32)
	__wait(source.read_async(vio.loader(), np.array([1, 0]), out))
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(out[0], stack[[1, 0]])
	assert np.array_equal(out[1], stack[[0, 2]])

def test_rows_of_locations_are_read_into_an_array_with_gaps(__written_stack):
	rows = [
		__locations(__written_stack, [0, 2]),
		__locations(__written_stack, [1, 0]),
	]
	source = vio.data.ImageSource(rows)
	out = np.zeros((2, 3, 4, 6), np.float32)[:, ::2]
	__wait(source.read_async(vio.loader(), np.array([0, 1]), out))
	assert np.array_equal(out[1, 0], __setup_array((3, 4, 6))[1])

def test_images_of_different_shapes_are_refused(__written_stack, tmp_path):
	other = tmp_path / 'other.mrcs'
	vio.write_stack(__setup_array((2, 5, 5)), other)
	locations = __locations(__written_stack, [0]) + __locations(other, [0])
	with pytest.raises(ValueError):
		vio.data.ImageSource(locations)

def test_an_index_past_the_end_of_a_stack_is_refused(__written_stack):
	with pytest.raises(ValueError):
		vio.data.ImageSource(__locations(__written_stack, [0, 3]))

def test_an_index_into_a_file_that_is_not_a_stack_is_refused(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((4, 6)), path)
	with pytest.raises(ValueError):
		vio.data.ImageSource([vio.ImageLocation(path, 0)])

def test_only_the_first_file_is_asked_when_told_not_to_check(
	__written_stack
):
	locations = __locations(__written_stack, [0, 3])
	source = vio.data.ImageSource(locations, check=False)
	assert source.shape == (4, 6)

def test_a_source_of_no_images_is_refused():
	with pytest.raises(ValueError):
		vio.data.ImageSource([])

def test_what_is_not_a_location_is_refused():
	with pytest.raises(TypeError):
		vio.data.ImageSource([3])

def test_a_subset_of_an_image_source_reads_its_own_rows(__written_stack):
	source = vio.data.ImageSource(__locations(__written_stack, [0, 1, 2]))
	subset = source.subset(np.array([2, 1]))
	out = np.empty((1, 4, 6), np.float32)
	__wait(subset.read_async(vio.loader(), np.array([0]), out))
	assert len(subset) == 2
	assert np.array_equal(out[0], __setup_array((3, 4, 6))[2])

def test_a_patch_source_crops_around_each_centre(__written_image):
	source = vio.data.PatchSource(
		[__written_image] * 2, [(10, 10), (20, 30)], (8, 8)
	)
	out = np.empty((2, 8, 8), np.float32)
	__wait(source.read_async(vio.loader(), np.array([1, 0]), out))
	image = __setup_array((32, 48))
	assert np.array_equal(out[0], image[16:24, 26:34])
	assert np.array_equal(out[1], image[6:14, 6:14])

def test_what_a_patch_has_beyond_its_image_is_the_fill(__written_image):
	source = vio.data.PatchSource([__written_image], [(0, 0)], (4, 4), fill=-1)
	out = np.empty((1, 4, 4), np.float32)
	__wait(source.read_async(vio.loader(), np.array([0]), out))
	assert (out[0, :2] == -1).all()
	assert (out[0, :, :2] == -1).all()
	assert np.array_equal(out[0, 2:, 2:], __setup_array((32, 48))[:2, :2])

def test_patches_of_several_images_land_in_their_rows(
	__written_image, __written_stack
):
	images = [
		vio.ImageLocation(__written_stack, 1),
		vio.ImageLocation(__written_image),
		vio.ImageLocation(__written_stack, 1),
	]
	source = vio.data.PatchSource(images, [(1, 1), (10, 10), (2, 3)], (2, 2))
	out = np.empty((3, 2, 2), np.float32)
	__wait(source.read_async(vio.loader(), np.array([0, 1, 2]), out))
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(out[0], stack[1, 0:2, 0:2])
	assert np.array_equal(out[1], __setup_array((32, 48))[9:11, 9:11])
	assert np.array_equal(out[2], stack[1, 1:3, 2:4])

def test_patches_take_the_type_asked_for(__written_image):
	source = vio.data.PatchSource(
		[__written_image], [(4, 4)], (2, 2), data_type=np.int32
	)
	assert source.dtype == np.int32

@pytest.mark.parametrize(
	"centres, shape",
	[
		pytest.param([(1, 1, 1)], (2, 2), id="Centre of another rank"),
		pytest.param([(1, 1)], (2, 2, 2), id="Patch of another rank"),
		pytest.param([(1, 1), (2, 2)], (2, 2), id="More centres than rows"),
		pytest.param([(-1, 1)], (2, 2), id="Negative centre"),
	]
)
def test_patches_that_do_not_fit_their_rows_are_refused(
	centres, shape, __written_image
):
	with pytest.raises(ValueError):
		vio.data.PatchSource([__written_image], centres, shape)

def test_a_patch_of_a_whole_stack_is_refused(__written_stack):
	with pytest.raises(ValueError):
		vio.data.PatchSource([__written_stack], [(1, 1)], (2, 2))

def test_a_subset_of_a_patch_source_reads_its_own_rows(__written_image):
	source = vio.data.PatchSource(
		[__written_image] * 2, [(10, 10), (20, 30)], (8, 8)
	)
	subset = source.subset(np.array([1]))
	out = np.empty((1, 8, 8), np.float32)
	__wait(subset.read_async(vio.loader(), np.array([0]), out))
	assert np.array_equal(out[0], __setup_array((32, 48))[16:24, 26:34])

def test_an_array_source_takes_the_rows_of_its_array():
	array = __setup_array((3, 2))
	source = vio.data.ArraySource(array)
	out = np.empty((2, 2), array.dtype)
	assert source.read_async(None, np.array([2, 2]), out) == []
	assert np.array_equal(out, array[[2, 2]])
	assert source.shape == (2,)

def test_an_array_source_of_a_single_value_is_refused():
	with pytest.raises(ValueError):
		vio.data.ArraySource(np.float32(1))

def __wait(pending):
	for read in pending:
		read.get()

def __locations(path, indices):
	return [vio.ImageLocation(path, index) for index in indices]

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	return path

@pytest.fixture
def __written_image(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((32, 48)), path)
	return path
