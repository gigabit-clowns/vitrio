# SPDX-License-Identifier: LGPL-2.1-or-later

# What reads and writes an array from Python works over memory on loan. No
# thread of that work releases anything of Python: the completion handed
# back returns the memory, and it waits for the work before it does.

import gc
import os
import subprocess
import sys
import textwrap
import weakref

import numpy as np
import pytest

import vitrio as vio

BATCH_SIZE = 64

def test_a_completion_keeps_its_destination_alive(__written_files):
	loader = vio.loader()
	destination = np.zeros((BATCH_SIZE, 4, 6), dtype=np.float32)
	alive = weakref.ref(destination)
	completion = vio.read_batch_async(loader, destination, __written_files)
	del destination
	gc.collect()
	assert alive() is not None
	completion.get()
	assert np.array_equal(alive()[BATCH_SIZE - 1], __setup_array((4, 6)))

def test_a_destination_is_released_with_its_completion(__written_files):
	loader = vio.loader()
	destination = np.zeros((BATCH_SIZE, 4, 6), dtype=np.float32)
	alive = weakref.ref(destination)
	completion = vio.read_batch_async(loader, destination, __written_files)
	completion.get()
	del destination, completion
	gc.collect()
	assert alive() is None

def test_dropping_a_completion_waits_for_its_reads(__written_files):
	loader = vio.loader()
	destination = np.zeros((BATCH_SIZE, 4, 6), dtype=np.float32)
	vio.read_batch_async(loader, destination, __written_files)
	assert np.array_equal(destination[BATCH_SIZE - 1], __setup_array((4, 6)))

def test_a_completion_outlives_the_loader_and_its_executor(__written_files):
	loader = vio.loader()
	destination = np.zeros((BATCH_SIZE, 4, 6), dtype=np.float32)
	completion = vio.read_batch_async(loader, destination, __written_files)
	del loader
	gc.collect()
	completion.get()
	assert np.array_equal(destination[0], __setup_array((4, 6)))

def test_a_refused_read_keeps_nothing(__written_files):
	loader = vio.loader()
	destination = np.zeros((BATCH_SIZE + 1, 4, 6), dtype=np.float32)
	alive = weakref.ref(destination)
	with pytest.raises(ValueError):
		vio.read_batch_async(loader, destination, __written_files)
	del destination
	gc.collect()
	assert alive() is None

def test_a_failed_read_returns_its_destination(tmp_path):
	loader = vio.loader()
	destination = np.zeros((2, 4, 6), dtype=np.float32)
	alive = weakref.ref(destination)
	locations = [vio.ImageLocation(tmp_path / 'missing.mrc')] * 2
	completion = vio.read_batch_async(loader, destination, locations)
	with pytest.raises(RuntimeError):
		completion.get()
	del destination, completion
	gc.collect()
	assert alive() is None

def test_a_read_into_an_array_keeps_nothing_once_it_returns(__written_files):
	destination = np.zeros((4, 6), dtype=np.float32)
	references = sys.getrefcount(destination)
	vio.read(__written_files[0], out=destination)
	assert sys.getrefcount(destination) == references

def test_a_write_keeps_nothing_once_it_returns(tmp_path):
	source = __setup_array((4, 6))
	references = sys.getrefcount(source)
	vio.write_single(source, tmp_path / 'image.mrc')
	assert sys.getrefcount(source) == references

def test_a_completion_keeps_its_source_alive(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(
		path,
		vio.ImageDescriptor(
			(BATCH_SIZE, 4, 6), 2, vio.NumericalType.float32
		)
	)
	saver = vio.saver(writers)
	source = __setup_array((BATCH_SIZE, 4, 6))
	alive = weakref.ref(source)
	locations = [vio.ImageLocation(path, i) for i in range(BATCH_SIZE)]
	completion = vio.write_batch_async(saver, source, locations)
	del source
	gc.collect()
	assert alive() is not None
	completion.get()
	del completion
	gc.collect()
	assert alive() is None
	writers.close(path)
	last = vio.read(vio.ImageLocation(path, BATCH_SIZE - 1))
	assert np.array_equal(
		np.asarray(last), __setup_array((BATCH_SIZE, 4, 6))[-1]
	)

# A thread that is still reading when the interpreter shuts down must not
# need the interpreter for anything.
@pytest.mark.parametrize(
	'ending',
	[
		pytest.param('completion.get()', id="Waited for"),
		pytest.param('del completion', id="Dropped"),
		pytest.param('pass', id="Left in flight"),
	]
)
def test_the_interpreter_shuts_down_cleanly(ending, __written_files):
	script = textwrap.dedent(f'''
		import numpy as np
		import vitrio as vio

		loader = vio.loader()
		locations = [
			vio.ImageLocation({str(__written_files[0].key)!r})
		] * {BATCH_SIZE}
		destination = np.zeros(({BATCH_SIZE}, 4, 6), dtype=np.float32)
		completion = vio.read_batch_async(loader, destination, locations)
		{ending}
	''')
	for _ in range(10):
		result = subprocess.run(
			[sys.executable, '-c', script],
			env=os.environ, capture_output=True, text=True, timeout=120
		)
		assert result.returncode == 0, result.stderr
		assert result.stderr == ''

def __setup_array(shape):
	count = int(np.prod(shape))
	return np.arange(count, dtype=np.float32).reshape(shape)

# The same file many times over, so that a batch is long enough to still be
# in flight when the test goes on.
@pytest.fixture
def __written_files(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(__setup_array((4, 6)), path)
	return [vio.ImageLocation(path)] * BATCH_SIZE
