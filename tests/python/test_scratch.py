# SPDX-License-Identifier: LGPL-2.1-or-later

import os

import numpy as np
import pytest

import vitrio as vio

def test_reads_through_a_scratch_what_the_file_holds(__written_stack):
	locations = __locations(__written_stack, range(5))
	scratch = vio.create_host_image_scratch(locations, 2)
	loader = vio.loader(scratch=scratch)
	destination = np.zeros((5, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, locations).get()
	assert np.array_equal(destination, __setup_array())

def test_a_prefetched_image_is_read_from_the_scratch(__written_stack):
	locations = __locations(__written_stack, [1, 3])
	scratch = vio.create_host_image_scratch(locations, 2)
	executor = vio.ThreadPoolExecutor(2)
	vio.prefetch_scratch_async(scratch, locations, executor).get()

	__overwrite(__written_stack)
	readers = vio.reader_provider(scratch=scratch)
	assert np.array_equal(
		vio.read(locations[1], readers),
		__setup_array()[3]
	)

def test_an_image_read_through_the_scratch_is_taken_in(__written_stack):
	locations = __locations(__written_stack, [2])
	scratch = vio.create_host_image_scratch(locations, 1)
	readers = vio.reader_provider(scratch=scratch)
	vio.read(locations[0], readers)

	__overwrite(__written_stack)
	assert np.array_equal(vio.read(locations[0], readers), __setup_array()[2])

def test_an_image_the_scratch_does_not_hold_is_read_from_the_file(
	__written_stack
):
	locations = __locations(__written_stack, [1])
	scratch = vio.create_host_image_scratch(locations, 1)
	executor = vio.SynchronousExecutor()
	vio.prefetch_scratch_async(scratch, locations, executor).get()

	__overwrite(__written_stack)
	readers = vio.reader_provider(scratch=scratch)
	location = vio.ImageLocation(__written_stack, 0)
	assert np.array_equal(vio.read(location, readers), np.zeros((4, 6)))

def test_a_file_the_scratch_does_not_hold_is_read_from_the_file(
	__written_stack,
	tmp_path
):
	other = tmp_path / 'other.mrc'
	vio.write_single(__setup_array()[4], other)
	scratch = vio.create_host_image_scratch(
		__locations(__written_stack, [0]),
		1
	)
	readers = vio.reader_provider(scratch=scratch)
	assert np.array_equal(vio.read(other, readers), __setup_array()[4])

def test_holds_whole_files(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array()[0], path)
	locations = [vio.ImageLocation(path)]
	scratch = vio.create_host_image_scratch(locations, 1)
	executor = vio.SynchronousExecutor()
	vio.prefetch_scratch_async(scratch, locations, executor).get()

	vio.write_single(np.zeros((4, 6), dtype=np.float32), path)
	readers = vio.reader_provider(scratch=scratch)
	assert np.array_equal(vio.read(path, readers), __setup_array()[0])

def test_prefetching_on_a_synchronous_executor_is_done_on_return(
	__written_stack
):
	locations = __locations(__written_stack, range(5))
	scratch = vio.create_host_image_scratch(locations, 2)
	executor = vio.SynchronousExecutor()
	completion = vio.prefetch_scratch_async(scratch, locations, executor)
	assert completion.is_ready

def test_takes_a_grouping_of_the_locations(__written_stack):
	grouping = vio.ImageLocationGrouping(
		__locations(__written_stack, [0, 4])
	)
	scratch = vio.create_host_image_scratch(grouping, 2)
	executor = vio.ThreadPoolExecutor(2)
	vio.prefetch_scratch_async(scratch, grouping, executor).get()
	assert isinstance(scratch, vio.ImageScratch)

def test_prefetching_a_file_that_is_gone_is_reported(__written_stack):
	locations = __locations(__written_stack, [0])
	scratch = vio.create_host_image_scratch(locations, 1)
	os.remove(__written_stack)
	completion = vio.prefetch_scratch_async(
		scratch,
		locations,
		vio.ThreadPoolExecutor(1)
	)
	with pytest.raises(RuntimeError):
		completion.get()

def test_a_run_length_of_zero_is_refused(__written_stack):
	with pytest.raises(ValueError):
		vio.create_host_image_scratch(__locations(__written_stack, [0]), 0)

def test_no_locations_are_refused():
	with pytest.raises(ValueError):
		vio.create_host_image_scratch([], 1)

def test_a_maximum_size_with_room_for_no_image_is_refused(__written_stack):
	with pytest.raises(ValueError):
		vio.create_host_image_scratch(
			__locations(__written_stack, [0]),
			1,
			max_size=1
		)

def test_an_index_past_the_end_of_a_stack_is_refused(__written_stack):
	with pytest.raises(IndexError):
		vio.create_host_image_scratch(__locations(__written_stack, [5]), 1)

def test_a_file_that_is_not_there_is_refused(tmp_path):
	locations = [vio.ImageLocation(tmp_path / 'missing.mrc')]
	with pytest.raises(RuntimeError):
		vio.create_host_image_scratch(locations, 1)

def test_is_given_the_provider_that_opens_the_files(__written_stack):
	locations = __locations(__written_stack, [0])
	files = vio.reader_provider(cache=1)
	scratch = vio.create_host_image_scratch(locations, 1, files=files)
	executor = vio.SynchronousExecutor()
	vio.prefetch_scratch_async(scratch, locations, executor, files).get()

def test_a_provider_serving_through_a_scratch_is_assembled_by_hand(
	__written_stack
):
	locations = __locations(__written_stack, [3])
	files = vio.FileImageReaderProvider(
		vio.ImageFileReadFormatSelector.get_shared()
	)
	scratch = vio.create_host_image_scratch(locations, 1, files=files)
	readers = vio.ScratchImageReaderProvider(files, scratch)
	assert isinstance(readers, vio.ImageReaderProvider)
	assert np.array_equal(vio.read(locations[0], readers), __setup_array()[3])

def test_a_mapped_file_scratch_reads_what_the_file_holds(
	__written_stack,
	tmp_path
):
	locations = __locations(__written_stack, range(5))
	scratch = vio.create_mapped_file_image_scratch(
		locations,
		tmp_path / 'scratch.bin',
		2
	)
	loader = vio.loader(scratch=scratch)
	destination = np.zeros((5, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, locations).get()
	assert np.array_equal(destination, __setup_array())

def test_the_file_of_a_mapped_scratch_outlives_it(__written_stack, tmp_path):
	path = tmp_path / 'scratch.bin'
	scratch = vio.create_mapped_file_image_scratch(
		__locations(__written_stack, [0]),
		path,
		1
	)
	del scratch
	assert path.is_file()

def test_a_mapped_scratch_resumes_from_its_file(__written_stack, tmp_path):
	locations = __locations(__written_stack, [1, 2])
	path = tmp_path / 'scratch.bin'
	scratch = vio.create_mapped_file_image_scratch(locations, path, 2)
	executor = vio.SynchronousExecutor()
	vio.prefetch_scratch_async(scratch, locations, executor).get()
	del scratch

	# The values change, but the time the file was modified at does not,
	# so the scratch has no way of telling.
	status = os.stat(__written_stack)
	__overwrite(__written_stack)
	os.utime(__written_stack, ns=(status.st_atime_ns, status.st_mtime_ns))

	scratch = vio.create_mapped_file_image_scratch(locations, path, 2)
	readers = vio.reader_provider(scratch=scratch)
	assert np.array_equal(vio.read(locations[1], readers), __setup_array()[2])

def test_a_mapped_scratch_reads_again_a_modified_file(
	__written_stack,
	tmp_path
):
	locations = __locations(__written_stack, [1, 2])
	path = tmp_path / 'scratch.bin'
	scratch = vio.create_mapped_file_image_scratch(locations, path, 2)
	executor = vio.SynchronousExecutor()
	vio.prefetch_scratch_async(scratch, locations, executor).get()
	del scratch

	status = os.stat(__written_stack)
	__overwrite(__written_stack)
	later = status.st_mtime_ns + 10 ** 10
	os.utime(__written_stack, ns=(later, later))

	scratch = vio.create_mapped_file_image_scratch(locations, path, 2)
	readers = vio.reader_provider(scratch=scratch)
	assert np.array_equal(vio.read(locations[1], readers), np.zeros((4, 6)))

def test_a_mapped_scratch_with_nothing_to_hold_creates_no_file(tmp_path):
	path = tmp_path / 'scratch.bin'
	with pytest.raises(ValueError):
		vio.create_mapped_file_image_scratch([], path, 1)
	assert not path.exists()

def __setup_array():
	return np.arange(5 * 4 * 6, dtype=np.float32).reshape((5, 4, 6))

def __locations(path, indices):
	return [vio.ImageLocation(path, index) for index in indices]

# Gives the stack other values, of the same shape and data type, so that a
# read tells whether it came from the scratch or from the file.
def __overwrite(path):
	vio.write_stack(np.zeros((5, 4, 6), dtype=np.float32), path)

@pytest.fixture
def __written_stack(tmp_path):
	path = tmp_path / 'stack.mrcs'
	vio.write_stack(__setup_array(), path)
	return path
