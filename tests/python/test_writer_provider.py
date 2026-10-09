# SPDX-License-Identifier: LGPL-2.1-or-later

import os

import pytest

import vitrio as vio

def test_starts_with_no_file_declared():
	assert vio.writer_provider().file_count == 0

def test_returns_a_file_provider():
	writers = vio.writer_provider()
	assert isinstance(writers, vio.FileImageWriterProvider)
	assert isinstance(writers, vio.ImageWriterProvider)

def test_counts_the_files_it_was_declared(tmp_path):
	writers = vio.writer_provider()
	writers.declare(tmp_path / 'first.mrcs', __setup_descriptor())
	writers.declare(tmp_path / 'second.mrcs', __setup_descriptor())
	assert writers.file_count == 2

def test_arguments_are_accepted_by_name(tmp_path):
	writers = vio.writer_provider()
	writers.declare(
		key=tmp_path / 'stack.mrcs', descriptor=__setup_descriptor()
	)
	assert writers.file_count == 1

def test_a_key_is_the_same_as_a_string_and_as_a_path(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(str(path), __setup_descriptor())
	writers.close(path)
	assert writers.file_count == 0

def test_declaring_creates_no_file(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, __setup_descriptor())
	assert not os.path.exists(path)

def test_a_key_declared_twice_is_refused(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	descriptor = __setup_descriptor()
	writers.declare(path, descriptor)
	with pytest.raises(ValueError):
		writers.declare(path, descriptor)

def test_closing_forgets_the_file(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, __setup_descriptor())
	writers.close(path)
	assert writers.file_count == 0

def test_a_closed_key_can_be_declared_again(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, __setup_descriptor())
	writers.close(path)
	writers.declare(path, __setup_descriptor())
	assert writers.file_count == 1

def test_closing_a_key_that_was_not_declared_is_refused(tmp_path):
	writers = vio.writer_provider()
	with pytest.raises(IndexError):
		writers.close(tmp_path / 'stack.mrcs')

def test_flushing_with_nothing_written_creates_no_file(tmp_path):
	path = tmp_path / 'stack.mrcs'
	writers = vio.writer_provider()
	writers.declare(path, __setup_descriptor())
	writers.flush()
	assert not os.path.exists(path)

def test_accepts_explicit_formats(tmp_path):
	writers = vio.writer_provider(
		vio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(tmp_path / 'stack.mrcs', __setup_descriptor())
	assert writers.file_count == 1

def test_assembled_by_hand(tmp_path):
	writers = vio.FileImageWriterProvider(
		vio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(tmp_path / 'stack.mrcs', __setup_descriptor())
	assert writers.file_count == 1

def __setup_descriptor():
	return vio.ImageDescriptor((3, 4, 6), 2, vio.NumericalType.float32)
