# SPDX-License-Identifier: LGPL-2.1-or-later

import numpy
import pytest

import vitrio

STACK_DESCRIPTOR = vitrio.ImageDescriptor(
	(4, 4, 6), 2, vitrio.NumericalType.float32
)

def test_a_stack_written_a_batch_at_a_time_reads_back(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	stack = __setup_array((4, 4, 6))
	for first in (0, 2):
		locations = [
			vitrio.ImageLocation(path, first),
			vitrio.ImageLocation(path, first + 1),
		]
		batch = stack[first:first + 2]
		vitrio.write_batch_async(saver, batch, locations).get()
	writers.close(path)
	assert vitrio.query_descriptor(path) == STACK_DESCRIPTOR
	assert numpy.array_equal(numpy.asarray(vitrio.read(path)), stack)

def test_a_completion_reports_when_it_is_done(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	batch = __setup_array((2, 4, 6))
	completion = vitrio.write_batch_async(
		saver, batch, __setup_locations(path, 2)
	)
	completion.wait()
	assert completion.is_ready

def test_a_batch_spans_several_files(tmp_path):
	first = tmp_path / 'first.mrcs'
	second = tmp_path / 'second.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(first, STACK_DESCRIPTOR)
	writers.declare(second, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	batch = __setup_array((2, 4, 6))
	locations = [
		vitrio.ImageLocation(first, 0),
		vitrio.ImageLocation(second, 3),
	]
	vitrio.write_batch_async(saver, batch, locations).get()
	writers.flush()
	assert numpy.array_equal(
		numpy.asarray(vitrio.read(vitrio.ImageLocation(first, 0))), batch[0]
	)
	assert numpy.array_equal(
		numpy.asarray(vitrio.read(vitrio.ImageLocation(second, 3))), batch[1]
	)

def test_a_source_that_cannot_be_written_to_is_written_out(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	batch = __setup_array((2, 4, 6))
	batch.flags.writeable = False
	vitrio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	result = numpy.asarray(vitrio.read(vitrio.ImageLocation(path, 1)))
	assert numpy.array_equal(result, batch[1])

def test_an_empty_batch_is_already_done():
	saver = vitrio.saver(vitrio.writer_provider())
	batch = numpy.zeros((0, 4, 6), dtype=numpy.float32)
	assert vitrio.write_batch_async(saver, batch, []).is_ready

def test_a_source_of_the_wrong_batch_size_is_refused(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	batch = __setup_array((3, 4, 6))
	locations = __setup_locations(path, 2)
	with pytest.raises(ValueError):
		vitrio.write_batch_async(saver, batch, locations)

def test_a_key_that_was_not_declared_is_reported(tmp_path):
	path = tmp_path / 'stack.mrcs'
	saver = vitrio.saver(vitrio.writer_provider())
	batch = __setup_array((2, 4, 6))
	completion = vitrio.write_batch_async(
		saver, batch, __setup_locations(path, 2)
	)
	with pytest.raises(IndexError):
		completion.get()

def test_an_index_past_the_end_of_a_stack_is_reported(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers)
	batch = __setup_array((2, 4, 6))
	locations = [
		vitrio.ImageLocation(path, 0),
		vitrio.ImageLocation(path, 4),
	]
	completion = vitrio.write_batch_async(saver, batch, locations)
	with pytest.raises(IndexError):
		completion.get()

def test_runs_on_a_synchronous_executor_too(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.writer_provider()
	writers.declare(path, STACK_DESCRIPTOR)
	saver = vitrio.saver(writers, executor=vitrio.SynchronousExecutor())
	batch = __setup_array((2, 4, 6))
	vitrio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	assert vitrio.query_descriptor(path) == STACK_DESCRIPTOR

def test_saver_returns_an_image_saver():
	saver = vitrio.saver(vitrio.writer_provider(), workers=2)
	assert isinstance(saver, vitrio.ExecutorImageSaver)
	assert isinstance(saver, vitrio.ImageSaver)

def test_assembled_by_hand(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vitrio.FileImageWriterProvider(
		vitrio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(path, STACK_DESCRIPTOR)
	executor = vitrio.ThreadPoolExecutor(2)
	saver = vitrio.ExecutorImageSaver(writers, executor)
	batch = __setup_array((2, 4, 6))
	vitrio.write_batch_async(saver, batch, __setup_locations(path, 2)).get()
	writers.close(path)
	assert vitrio.query_descriptor(path) == STACK_DESCRIPTOR

def __setup_locations(path, count):
	return [vitrio.ImageLocation(path, index) for index in range(count)]

def __setup_array(shape):
	count = int(numpy.prod(shape))
	return numpy.arange(count, dtype=numpy.float32).reshape(shape)
