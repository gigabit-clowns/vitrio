# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy
import pytest

import vitrio

def test_round_trip_returns_an_array(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	assert isinstance(vitrio.read(path), vitrio.Array)

def test_a_single_image_reads_back_as_it_was_written(tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	vitrio.write_single(source, path)
	assert numpy.array_equal(numpy.asarray(vitrio.read(path)), source)

def test_a_path_is_taken_as_a_string_too(tmp_path):
	path = str(tmp_path / 'image.mrc')
	source = __setup_array((4, 6))
	vitrio.write_single(source, path)
	assert numpy.array_equal(numpy.asarray(vitrio.read(path)), source)

def test_an_array_written_single_is_one_volume(tmp_path):
	path = tmp_path / 'volume.mrc'
	vitrio.write_single(__setup_array((3, 4, 6)), path)
	assert vitrio.query_descriptor(path).core_rank == 3

def test_an_array_written_as_a_stack_is_a_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vitrio.write_stack(__setup_array((3, 4, 6)), path)
	descriptor = vitrio.query_descriptor(path)
	assert descriptor.extents == (3, 4, 6)
	assert descriptor.core_rank == 2

def test_a_stack_needs_an_axis_to_stack_along(tmp_path):
	with pytest.raises(ValueError):
		vitrio.write_stack(__setup_array((6,)), tmp_path / 'stack.mrcs')

@pytest.mark.parametrize(
	'data_type',
	[
		pytest.param(vitrio.NumericalType.int16, id="Member"),
		pytest.param('int16', id="Name"),
		pytest.param(numpy.int16, id="Type of numpy"),
		pytest.param(numpy.dtype('int16'), id="Dtype of numpy"),
	]
)
def test_writes_the_type_it_is_asked_for(data_type, tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path, data_type=data_type)
	descriptor = vitrio.query_descriptor(path)
	assert descriptor.data_type == vitrio.NumericalType.int16

def test_a_stack_is_written_as_the_type_it_is_asked_for(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vitrio.write_stack(__setup_array((3, 4, 6)), path, data_type='int16')
	descriptor = vitrio.query_descriptor(path)
	assert descriptor.data_type == vitrio.NumericalType.int16

def test_writes_what_a_descriptor_states(tmp_path):
	path = tmp_path / 'stack.mrcs'
	descriptor = vitrio.ImageDescriptor(
		(3, 4, 6), 2, vitrio.NumericalType.int16
	)
	vitrio.write(__setup_array((3, 4, 6)), path, descriptor)
	assert vitrio.query_descriptor(path) == descriptor

def test_a_descriptor_of_another_shape_is_refused(tmp_path):
	descriptor = vitrio.ImageDescriptor(
		(2, 4, 6), 2, vitrio.NumericalType.float32
	)
	with pytest.raises(ValueError):
		vitrio.write(
			__setup_array((3, 4, 6)), tmp_path / 'stack.mrcs', descriptor
		)

def test_reads_the_type_the_file_holds(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path, data_type='int16')
	assert vitrio.read(path).data_type == vitrio.NumericalType.int16

@pytest.mark.parametrize(
	'data_type',
	[
		pytest.param(vitrio.NumericalType.float64, id="Member"),
		pytest.param('float64', id="Name"),
		pytest.param(numpy.float64, id="Type of numpy"),
	]
)
def test_reads_the_type_it_is_asked_for(data_type, tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	vitrio.write_single(source, path, data_type='int16')
	result = vitrio.read(path, data_type=data_type)
	assert result.data_type == vitrio.NumericalType.float64
	assert numpy.array_equal(numpy.asarray(result), source)

def test_reads_a_whole_file_through_an_image_location(tmp_path):
	path = tmp_path / 'stack.mrcs'
	source = __setup_array((3, 4, 6))
	vitrio.write_stack(source, path)
	result = vitrio.read(vitrio.ImageLocation(path))
	assert numpy.array_equal(numpy.asarray(result), source)

def test_reads_one_image_of_a_stack_through_an_image_location(tmp_path):
	path = tmp_path / 'stack.mrcs'
	source = __setup_array((3, 4, 6))
	vitrio.write_stack(source, path)
	result = vitrio.read(vitrio.ImageLocation(path, 1))
	assert numpy.array_equal(numpy.asarray(result), source[1])

def test_what_read_returns_can_be_written_back(tmp_path):
	source = __setup_array((4, 6))
	vitrio.write_single(source, tmp_path / 'image.mrc')
	vitrio.write_single(
		vitrio.read(tmp_path / 'image.mrc'), tmp_path / 'copy.mrc'
	)
	result = vitrio.read(tmp_path / 'copy.mrc')
	assert numpy.array_equal(numpy.asarray(result), source)

def test_reads_into_the_array_it_is_given(tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	vitrio.write_single(source, path)
	destination = numpy.zeros((4, 6), dtype=numpy.float32)
	vitrio.read(path, out=destination)
	assert numpy.array_equal(destination, source)

def test_returns_the_array_it_was_given_to_read_into(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	destination = numpy.zeros((4, 6), dtype=numpy.float32)
	assert vitrio.read(path, out=destination) is destination

def test_reads_one_image_of_a_stack_into_the_array_it_is_given(tmp_path):
	path = tmp_path / 'stack.mrcs'
	source = __setup_array((3, 4, 6))
	vitrio.write_stack(source, path)
	destination = numpy.zeros((4, 6), dtype=numpy.float32)
	vitrio.read(vitrio.ImageLocation(path, 2), out=destination)
	assert numpy.array_equal(destination, source[2])

def test_reads_as_the_type_of_the_array_it_is_given(tmp_path):
	path = tmp_path / 'image.mrc'
	source = __setup_array((4, 6))
	vitrio.write_single(source, path)
	destination = numpy.zeros((4, 6), dtype=numpy.float64)
	vitrio.read(path, out=destination)
	assert numpy.array_equal(destination, source)

def test_an_array_of_another_shape_is_not_read_into(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	with pytest.raises(ValueError):
		vitrio.read(path, out=numpy.zeros((4, 5), dtype=numpy.float32))

def test_a_data_type_is_not_taken_with_an_array_to_read_into(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	destination = numpy.zeros((4, 6), dtype=numpy.float32)
	with pytest.raises(ValueError):
		vitrio.read(path, data_type='float32', out=destination)

def test_raises_on_a_file_that_is_not_there(tmp_path):
	with pytest.raises(RuntimeError):
		vitrio.read(tmp_path / 'missing.mrc')

def test_a_location_string_is_not_parsed_by_read(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	with pytest.raises(RuntimeError):
		vitrio.read(f'1@{path}')

def test_accepts_explicit_formats_and_an_explicit_provider(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(
		__setup_array((4, 6)), path,
		formats=vitrio.ImageFileWriteFormatSelector.get_shared()
	)
	readers = vitrio.FileImageReaderProvider(
		vitrio.ImageFileReadFormatSelector.get_shared()
	)
	assert isinstance(vitrio.read(path, readers=readers), vitrio.Array)

def test_a_provider_assembled_with_a_cache_serves_reads(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(__setup_array((4, 6)), path)
	readers = vitrio.reader_provider(cache=2)
	vitrio.read(path, readers=readers)
	vitrio.read(path, readers=readers)
	assert readers.reader_count == 1

def __setup_array(shape):
	count = int(numpy.prod(shape))
	return numpy.arange(count, dtype=numpy.float32).reshape(shape)
