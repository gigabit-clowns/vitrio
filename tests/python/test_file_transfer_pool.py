# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import subprocess
import sys
import textwrap

import numpy as np
import pytest

import vitrio as vio

# Reads a file, checks what it holds, and prints how many io_uring instances
# the process holds then.
_READ_AND_COUNT_RINGS = textwrap.dedent('''
	import os
	import sys

	import numpy as np
	import vitrio as vio

	read = np.asarray(vio.read(sys.argv[1]))
	assert np.array_equal(read, np.arange(24, dtype=np.float32).reshape(4, 6))

	rings = 0
	for entry in os.listdir('/proc/self/fd'):
		try:
			target = os.readlink(f'/proc/self/fd/{entry}')
		except OSError:
			continue
		rings += target == 'anon_inode:[io_uring]'
	print(rings)
''')

def _rings_after_a_read(path, **variables) -> int:
	result = subprocess.run(
		[sys.executable, '-c', _READ_AND_COUNT_RINGS, str(path)],
		env={**os.environ, **variables}, check=True, capture_output=True,
		text=True,
	)
	return int(result.stdout)

def _write_image(tmp_path):
	path = tmp_path / 'image.mrc'
	vio.write_single(np.arange(24, dtype=np.float32).reshape(4, 6), path)
	return path

_COUNTS_RINGS = pytest.mark.skipif(
	not os.path.isdir('/proc/self/fd'),
	reason='The system does not list the files of a process.',
)

def test_a_file_transfer_pool_needs_an_executor():
	with pytest.raises(TypeError):
		vio.FileTransferPool(None)

def test_a_provider_reads_through_the_pool_it_is_given(tmp_path):
	path = tmp_path / 'image.mrc'
	written = np.arange(24, dtype=np.float32).reshape(4, 6)
	vio.write_single(written, path)

	transfers = vio.FileTransferPool(vio.ThreadPoolExecutor(2))
	readers = vio.reader_provider(transfers=transfers)
	assert np.array_equal(np.asarray(vio.read(path, readers=readers)), written)

def test_a_reader_reads_after_its_pool_is_gone(tmp_path):
	path = tmp_path / 'image.mrc'
	written = np.arange(24, dtype=np.int16).reshape(4, 6)
	vio.write_single(written, path)

	readers = vio.FileImageReaderProvider(
		vio.ImageFileReadFormatSelector.get_shared(),
		vio.FileTransferPool(vio.ThreadPoolExecutor(2))
	)
	read = vio.read(path, readers=readers, data_type=np.float32)
	assert np.array_equal(np.asarray(read), written.astype(np.float32))

@pytest.mark.skipif(not hasattr(os, 'fork'), reason='The system has no fork.')
def test_the_child_of_a_fork_reads_through_a_pool_of_its_own(tmp_path):
	path = tmp_path / 'image.mrc'
	written = np.arange(24, dtype=np.float32).reshape(4, 6)
	vio.write_single(written, path)

	# The parent reads first, so the pool shared by the process is made
	# before the fork, with threads the child does not have.
	assert np.array_equal(np.asarray(vio.read(path)), written)

	child = os.fork()
	if child == 0:
		read = np.asarray(vio.read(path))
		os._exit(0 if np.array_equal(read, written) else 1)

	_, status = os.waitpid(child, 0)
	assert os.WIFEXITED(status)
	assert os.WEXITSTATUS(status) == 0

@_COUNTS_RINGS
def test_the_environment_sets_the_threads_of_file_transfers(tmp_path):
	path = _write_image(tmp_path)

	# One ring a thread where the process can have asynchronous ports, and
	# none where it can not.
	per_thread = _rings_after_a_read(path, VITRIO_FILE_TRANSFER_WORKERS='1')
	threads = _rings_after_a_read(path, VITRIO_FILE_TRANSFER_WORKERS='3')
	assert threads == 3 * per_thread

@_COUNTS_RINGS
def test_the_environment_turns_asynchronous_io_off(tmp_path):
	path = _write_image(tmp_path)
	assert _rings_after_a_read(path, VITRIO_DISABLE_ASYNCHRONOUS_IO='1') == 0
