# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

def test_reads_a_batch(__written_files):
	loader = vio.loader()
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, __written_files).get()
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def test_batches_in_flight_can_be_collected_together(__written_files):
	loader = vio.loader()
	first = np.zeros((3, 4, 6), dtype=np.float32)
	second = np.zeros((3, 4, 6), dtype=np.float32)
	completions = [
		vio.read_batch_async(loader, first, __written_files),
		vio.read_batch_async(loader, second, __written_files),
	]
	for completion in completions:
		completion.get()
	assert all(c.is_ready for c in completions)
	assert np.array_equal(first, second)

def test_a_completion_reports_when_it_is_done(__written_files):
	loader = vio.loader()
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	completion = vio.read_batch_async(loader, destination, __written_files)
	completion.wait()
	assert completion.is_ready

def test_returns_a_completion(__written_files):
	loader = vio.loader()
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	completion = vio.read_batch_async(loader, destination, __written_files)
	assert isinstance(completion, vio.Completion)

def test_an_empty_batch_is_already_done():
	loader = vio.loader()
	destination = np.zeros((0, 4, 6), dtype=np.float32)
	assert vio.read_batch_async(loader, destination, []).is_ready

def test_a_destination_of_the_wrong_batch_size_is_refused(__written_files):
	loader = vio.loader()
	destination = np.zeros((4, 4, 6), dtype=np.float32)
	with pytest.raises(ValueError):
		vio.read_batch_async(loader, destination, __written_files)

def test_reads_the_images_of_a_stack_by_their_index(__written_stack):
	loader = vio.loader()
	locations = [
		vio.ImageLocation(__written_stack, 2),
		vio.ImageLocation(__written_stack, 0),
	]
	destination = np.zeros((2, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, locations).get()
	stack = __setup_array((3, 4, 6))
	assert np.array_equal(destination[0], stack[2])
	assert np.array_equal(destination[1], stack[0])

def test_reads_as_the_type_of_the_destination(__written_files):
	loader = vio.loader()
	destination = np.zeros((3, 4, 6), dtype=np.float64)
	vio.read_batch_async(loader, destination, __written_files).get()
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def test_an_index_past_the_end_of_a_stack_is_reported(__written_stack):
	loader = vio.loader()
	locations = [
		vio.ImageLocation(__written_stack, 0),
		vio.ImageLocation(__written_stack, 3),
	]
	destination = np.zeros((2, 4, 6), dtype=np.float32)
	completion = vio.read_batch_async(loader, destination, locations)
	with pytest.raises(IndexError):
		completion.get()

def test_locations_with_and_without_an_index_do_not_mix(__written_stack):
	loader = vio.loader()
	locations = [
		vio.ImageLocation(__written_stack, 0),
		vio.ImageLocation(__written_stack),
	]
	destination = np.zeros((2, 4, 6), dtype=np.float32)
	with pytest.raises(ValueError):
		vio.read_batch_async(loader, destination, locations)

def test_runs_on_a_synchronous_executor_too(__written_files):
	loader = vio.loader(executor=vio.SynchronousExecutor())
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, __written_files).get()
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def test_a_caching_provider_serves_repeated_reads(__written_files):
	loader = vio.loader(cache=4)
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, __written_files).get()
	destination[...] = 0
	vio.read_batch_async(loader, destination, __written_files).get()
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def test_loader_returns_an_image_loader():
	loader = vio.loader(workers=2)
	assert isinstance(loader, vio.ExecutorImageLoader)
	assert isinstance(loader, vio.ImageLoader)

def test_assembled_by_hand(__written_files):
	readers = vio.FileImageReaderProvider(
		vio.ImageFileReadFormatSelector.get_shared()
	)
	executor = vio.ThreadPoolExecutor(2)
	loader = vio.ExecutorImageLoader(readers, executor)
	destination = np.zeros((3, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, __written_files).get()
	assert np.array_equal(destination, __setup_array((3, 4, 6)))

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

# Three files that, read as a batch, hold what a stack of three would.
@pytest.fixture
def __written_files(tmp_path):
	locations = []
	for index, image in enumerate(__setup_array((3, 4, 6))):
		path = tmp_path / f'image{index}.mrc'
		vio.write_single(image, path)
		locations.append(vio.ImageLocation(path))
	return locations

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array((3, 4, 6)), path)
	return path
