# SPDX-License-Identifier: LGPL-2.1-or-later

import os

import numpy as np
import pytest

import vitrio as vio

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
