# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy
import pytest

import vitrio

def test_reads_a_batch(__written_files):
	loader = vitrio.loader()
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read_batch_async(loader, destination, __written_files).get()
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def test_batches_in_flight_can_be_collected_together(__written_files):
	loader = vitrio.loader()
	first = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	second = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	completions = [
		vitrio.read_batch_async(loader, first, __written_files),
		vitrio.read_batch_async(loader, second, __written_files),
	]
	for completion in completions:
		completion.get()
	assert all(c.is_ready for c in completions)
	assert numpy.array_equal(first, second)

def test_a_completion_reports_when_it_is_done(__written_files):
	loader = vitrio.loader()
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	completion = vitrio.read_batch_async(loader, destination, __written_files)
	completion.wait()
	assert completion.is_ready

def test_returns_a_completion(__written_files):
	loader = vitrio.loader()
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	completion = vitrio.read_batch_async(loader, destination, __written_files)
	assert isinstance(completion, vitrio.Completion)

def test_an_empty_batch_is_already_done():
	loader = vitrio.loader()
	destination = numpy.zeros((0, 4, 6), dtype=numpy.float32)
	assert vitrio.read_batch_async(loader, destination, []).is_ready

def test_a_destination_of_the_wrong_batch_size_is_refused(__written_files):
	loader = vitrio.loader()
	destination = numpy.zeros((4, 4, 6), dtype=numpy.float32)
	with pytest.raises(ValueError):
		vitrio.read_batch_async(loader, destination, __written_files)

def test_reads_the_images_of_a_stack_by_their_index(__written_stack):
	loader = vitrio.loader()
	locations = [
		vitrio.ImageLocation(__written_stack, 2),
		vitrio.ImageLocation(__written_stack, 0),
	]
	destination = numpy.zeros((2, 4, 6), dtype=numpy.float32)
	vitrio.read_batch_async(loader, destination, locations).get()
	stack = __setup_array((3, 4, 6))
	assert numpy.array_equal(destination[0], stack[2])
	assert numpy.array_equal(destination[1], stack[0])

def test_reads_as_the_type_of_the_destination(__written_files):
	loader = vitrio.loader()
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float64)
	vitrio.read_batch_async(loader, destination, __written_files).get()
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def test_an_index_past_the_end_of_a_stack_is_reported(__written_stack):
	loader = vitrio.loader()
	locations = [
		vitrio.ImageLocation(__written_stack, 0),
		vitrio.ImageLocation(__written_stack, 3),
	]
	destination = numpy.zeros((2, 4, 6), dtype=numpy.float32)
	completion = vitrio.read_batch_async(loader, destination, locations)
	with pytest.raises(IndexError):
		completion.get()

def test_locations_with_and_without_an_index_do_not_mix(__written_stack):
	loader = vitrio.loader()
	locations = [
		vitrio.ImageLocation(__written_stack, 0),
		vitrio.ImageLocation(__written_stack),
	]
	destination = numpy.zeros((2, 4, 6), dtype=numpy.float32)
	with pytest.raises(ValueError):
		vitrio.read_batch_async(loader, destination, locations)

def test_runs_on_a_synchronous_executor_too(__written_files):
	loader = vitrio.loader(executor=vitrio.SynchronousExecutor())
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read_batch_async(loader, destination, __written_files).get()
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def test_a_caching_provider_serves_repeated_reads(__written_files):
	loader = vitrio.loader(cache=4)
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read_batch_async(loader, destination, __written_files).get()
	destination[...] = 0
	vitrio.read_batch_async(loader, destination, __written_files).get()
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def test_loader_returns_an_image_loader():
	loader = vitrio.loader(workers=2)
	assert isinstance(loader, vitrio.ExecutorImageLoader)
	assert isinstance(loader, vitrio.ImageLoader)

def test_assembled_by_hand(__written_files):
	readers = vitrio.FileImageReaderProvider(
		vitrio.ImageFileReadFormatSelector.get_shared()
	)
	executor = vitrio.ThreadPoolExecutor(2)
	loader = vitrio.ExecutorImageLoader(readers, executor)
	destination = numpy.zeros((3, 4, 6), dtype=numpy.float32)
	vitrio.read_batch_async(loader, destination, __written_files).get()
	assert numpy.array_equal(destination, __setup_array((3, 4, 6)))

def __setup_array(shape):
	count = int(numpy.prod(shape))
	return numpy.arange(count, dtype=numpy.float32).reshape(shape)

# Three files that, read as a batch, hold what a stack of three would.
@pytest.fixture
def __written_files(tmp_path):
	locations = []
	for index, image in enumerate(__setup_array((3, 4, 6))):
		path = tmp_path / f'image{index}.mrc'
		vitrio.write_single(image, path)
		locations.append(vitrio.ImageLocation(path))
	return locations

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vitrio.write_stack(__setup_array((3, 4, 6)), path)
	return path
