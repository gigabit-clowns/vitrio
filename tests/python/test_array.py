# SPDX-License-Identifier: LGPL-2.1-or-later

import gc
import sys

import numpy
import pytest

import vitrio

NumericalType = vitrio.NumericalType

DATA_TYPES = [
	(NumericalType.boolean, numpy.bool_),
	(NumericalType.int8, numpy.int8),
	(NumericalType.uint8, numpy.uint8),
	(NumericalType.int16, numpy.int16),
	(NumericalType.uint16, numpy.uint16),
	(NumericalType.int32, numpy.int32),
	(NumericalType.uint32, numpy.uint32),
	(NumericalType.int64, numpy.int64),
	(NumericalType.uint64, numpy.uint64),
	(NumericalType.float16, numpy.float16),
	(NumericalType.float32, numpy.float32),
	(NumericalType.float64, numpy.float64),
	(NumericalType.complex_float32, numpy.complex64),
	(NumericalType.complex_float64, numpy.complex128),
]

def test_is_not_constructed_from_python():
	with pytest.raises(TypeError):
		vitrio.Array()

def test_has_the_shape_of_what_was_read(__written_image):
	assert vitrio.read(__written_image).shape == (4, 6)

def test_the_shape_is_a_tuple(__written_image):
	assert isinstance(vitrio.read(__written_image).shape, tuple)

def test_has_the_data_type_of_what_was_read(__written_image):
	array = vitrio.read(__written_image)
	assert array.data_type == NumericalType.float32

def test_its_length_is_its_first_extent(__written_image):
	assert len(vitrio.read(__written_image)) == 4

def test_repr_states_the_shape_and_the_data_type(__written_image):
	assert repr(vitrio.read(__written_image)) == (
		'Array(shape=(4, 6), data_type=NumericalType.float32)'
	)

@pytest.mark.parametrize(('data_type', 'dtype'), DATA_TYPES)
def test_numpy_sees_the_data_type_through_a_buffer(
	data_type, dtype, __written_image
):
	array = vitrio.read(__written_image, data_type=data_type)
	assert numpy.asarray(array).dtype == dtype

@pytest.mark.parametrize(('data_type', 'dtype'), DATA_TYPES)
def test_numpy_sees_the_data_type_through_dlpack(
	data_type, dtype, __written_image
):
	array = vitrio.read(__written_image, data_type=data_type)
	assert numpy.from_dlpack(array).dtype == dtype

def test_numpy_sees_the_shape_and_the_values(__written_image):
	array = vitrio.read(__written_image)
	assert numpy.array_equal(numpy.asarray(array), __setup_array((4, 6)))

def test_dlpack_gives_the_shape_and_the_values(__written_image):
	array = vitrio.read(__written_image)
	assert numpy.array_equal(numpy.from_dlpack(array), __setup_array((4, 6)))

def test_memoryview_describes_the_array(__written_image):
	view = memoryview(vitrio.read(__written_image))
	assert view.format == 'f'
	assert view.itemsize == 4
	assert view.shape == (4, 6)
	assert view.strides == (24, 4)
	assert view.c_contiguous
	assert not view.readonly

def test_the_memory_is_shared_and_not_copied(__written_image):
	array = vitrio.read(__written_image)
	assert numpy.shares_memory(numpy.asarray(array), numpy.asarray(array))

def test_a_buffer_and_dlpack_give_the_same_memory(__written_image):
	array = vitrio.read(__written_image)
	assert numpy.shares_memory(
		numpy.asarray(array), numpy.from_dlpack(array)
	)

def test_a_write_through_one_view_is_seen_through_another(__written_image):
	array = vitrio.read(__written_image)
	numpy.asarray(array)[1, 2] = -5
	assert numpy.from_dlpack(array)[1, 2] == -5

def test_a_view_keeps_the_memory_alive(__written_image):
	view = numpy.asarray(vitrio.read(__written_image))
	gc.collect()
	assert numpy.array_equal(view, __setup_array((4, 6)))

def test_a_dlpack_consumer_keeps_the_memory_alive(__written_image):
	view = numpy.from_dlpack(vitrio.read(__written_image))
	gc.collect()
	assert numpy.array_equal(view, __setup_array((4, 6)))

def test_a_capsule_nobody_takes_lets_go_of_the_array(__written_image):
	array = vitrio.read(__written_image)
	references = sys.getrefcount(array)
	capsule = array.__dlpack__()
	assert sys.getrefcount(array) > references
	del capsule
	gc.collect()
	assert sys.getrefcount(array) == references

def test_reports_the_host_as_its_device(__written_image):
	assert vitrio.read(__written_image).__dlpack_device__() == (1, 0)

def test_the_device_the_array_is_on_can_be_asked_for(__written_image):
	array = vitrio.read(__written_image)
	assert array.__dlpack__(dl_device=(1, 0)) is not None

def test_another_device_is_refused(__written_image):
	array = vitrio.read(__written_image)
	with pytest.raises(BufferError):
		array.__dlpack__(dl_device=(2, 0))

def test_a_copy_is_refused(__written_image):
	array = vitrio.read(__written_image)
	with pytest.raises(BufferError):
		array.__dlpack__(copy=True)

def test_numpy_takes_a_versioned_capsule(__written_image):
	array = vitrio.read(__written_image)
	view = numpy.from_dlpack(array, copy=False)
	assert numpy.shares_memory(view, numpy.asarray(array))

# numpy has no complex number of half precision, so it is numpy that
# refuses one, whichever way it is handed over.
def test_numpy_refuses_the_type_it_does_not_have(__written_image):
	array = vitrio.read(
		__written_image, data_type=NumericalType.complex_float16
	)
	with pytest.raises((RuntimeError, TypeError, ValueError, BufferError)):
		numpy.asarray(array)
	with pytest.raises((RuntimeError, TypeError, ValueError, BufferError)):
		numpy.from_dlpack(array)

def __setup_array(shape):
	count = int(numpy.prod(shape))
	return numpy.arange(count, dtype=numpy.float32).reshape(shape)

@pytest.fixture
def __written_image(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	return path
