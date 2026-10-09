# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy
import pytest

import vitrio

def test_reports_what_a_single_image_was_written_with(__written_image):
	descriptor = vitrio.query_descriptor(__written_image)
	assert descriptor.extents == (4, 6)
	assert descriptor.core_rank == 2
	assert descriptor.data_type == vitrio.NumericalType.float32

def test_reports_a_stack_with_the_axis_it_stacks_along_first(__written_stack):
	descriptor = vitrio.query_descriptor(__written_stack)
	assert descriptor.extents == (3, 4, 6)
	assert descriptor.core_rank == 2

def test_returns_an_image_descriptor(__written_image):
	descriptor = vitrio.query_descriptor(__written_image)
	assert isinstance(descriptor, vitrio.ImageDescriptor)

def test_takes_the_key_as_a_string_too(__written_image):
	assert vitrio.query_descriptor(str(__written_image)).extents == (4, 6)

def test_what_it_reports_sizes_a_destination(__written_stack):
	descriptor = vitrio.query_descriptor(__written_stack)
	core = vitrio.get_core_extents(descriptor)
	destination = numpy.zeros((2, *core), dtype=numpy.float32)
	assert destination.shape[1:] == core

def test_accepts_an_explicit_provider(__written_image):
	readers = vitrio.FileImageReaderProvider(
		vitrio.ImageFileReadFormatSelector.get_shared()
	)
	descriptor = vitrio.query_descriptor(__written_image, readers)
	assert descriptor.extents == (4, 6)

def test_a_caching_provider_keeps_the_reader_it_opened(__written_image):
	readers = vitrio.reader_provider(cache=4)
	vitrio.query_descriptor(__written_image, readers)
	vitrio.query_descriptor(__written_image, readers)
	assert readers.capacity == 4
	assert readers.reader_count == 1

def test_raises_on_a_file_that_is_not_there(tmp_path):
	with pytest.raises(RuntimeError):
		vitrio.query_descriptor(tmp_path / 'missing.mrc')

@pytest.fixture
def __written_image(tmp_path):
	path = tmp_path / 'image.mrc'
	vitrio.write_single(numpy.zeros((4, 6), dtype=numpy.float32), path)
	return path

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vitrio.write_stack(numpy.zeros((3, 4, 6), dtype=numpy.float32), path)
	return path
