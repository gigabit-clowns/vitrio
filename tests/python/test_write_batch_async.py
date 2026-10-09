# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy as np
import pytest

import vitrio as vio

STACK_DESCRIPTOR = vio.ImageDescriptor(
	(4, 4, 6), 2, vio.NumericalType.float32
)

def test_a_stack_written_a_batch_at_a_time_reads_back(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	stack = __setup_array((4, 4, 6))
	for first in (0, 2):
		locations = [
			vio.ImageLocation(path, first),
			vio.ImageLocation(path, first + 1),
		]
		batch = stack[first:first + 2]
		vio.write_batch_async(saver, batch, locations).get()
	writers.close(path)
	assert vio.query_descriptor(path) == STACK_DESCRIPTOR
	assert np.array_equal(np.asarray(vio.read(path)), stack)

def test_a_completion_reports_when_it_is_done(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	batch = __setup_array((2, 4, 6))
	completion = vio.write_batch_async(
		saver, batch, __setup_locations(path, 2)
	)
	completion.wait()
	assert completion.is_ready

def test_a_batch_spans_several_files(tmp_path):
	first = tmp_path / 'first.mrcs'
	second = tmp_path / 'second.mrcs'
	writers = vio.writer_provider()
	writers.declare(first, STACK_DESCRIPTOR)
	writers.declare(second, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	batch = __setup_array((2, 4, 6))
	locations = [
		vio.ImageLocation(first, 0),
		vio.ImageLocation(second, 3),
	]
	vio.write_batch_async(saver, batch, locations).get()
	writers.flush()
	assert np.array_equal(
		np.asarray(vio.read(vio.ImageLocation(first, 0))), batch[0]
	)
	assert np.array_equal(
		np.asarray(vio.read(vio.ImageLocation(second, 3))), batch[1]
	)

def test_a_source_that_cannot_be_written_to_is_written_out(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	batch = __setup_array((2, 4, 6))
	batch.flags.writeable = False
	vio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	result = np.asarray(vio.read(vio.ImageLocation(path, 1)))
	assert np.array_equal(result, batch[1])

def test_an_empty_batch_is_already_done():
	saver = vio.saver(vio.writer_provider())
	batch = np.zeros((0, 4, 6), dtype=np.float32)
	assert vio.write_batch_async(saver, batch, []).is_ready

def test_a_source_of_the_wrong_batch_size_is_refused(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	batch = __setup_array((3, 4, 6))
	locations = __setup_locations(path, 2)
	with pytest.raises(ValueError):
		vio.write_batch_async(saver, batch, locations)

def test_a_key_that_was_not_declared_is_reported(tmp_path):
	path = tmp_path / 'stack.mrcs'
	saver = vio.saver(vio.writer_provider())
	batch = __setup_array((2, 4, 6))
	completion = vio.write_batch_async(
		saver, batch, __setup_locations(path, 2)
	)
	with pytest.raises(IndexError):
		completion.get()

def test_an_index_past_the_end_of_a_stack_is_reported(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers)
	batch = __setup_array((2, 4, 6))
	locations = [
		vio.ImageLocation(path, 0),
		vio.ImageLocation(path, 4),
	]
	completion = vio.write_batch_async(saver, batch, locations)
	with pytest.raises(IndexError):
		completion.get()

def test_runs_on_a_synchronous_executor_too(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vio.saver(writers, executor=vio.SynchronousExecutor())
	batch = __setup_array((2, 4, 6))
	vio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	assert vio.query_descriptor(path) == STACK_DESCRIPTOR

def test_saver_returns_an_image_saver():
	saver = vio.saver(vio.writer_provider(), workers=2)
	assert isinstance(saver, vio.ExecutorImageSaver)
	assert isinstance(saver, vio.ImageSaver)

def test_assembled_by_hand(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.FileImageWriterProvider(
		vio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(path, STACK_DESCRIPTOR)
	executor = vio.ThreadPoolExecutor(2)
	saver = vio.ExecutorImageSaver(writers, executor)
	batch = __setup_array((2, 4, 6))
	vio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	assert vio.query_descriptor(path) == STACK_DESCRIPTOR

def __setup_locations(path, count):
	return [vio.ImageLocation(path, index) for index in range(count)]

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)
