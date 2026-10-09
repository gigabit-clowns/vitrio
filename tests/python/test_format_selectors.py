# SPDX-License-Identifier: LGPL-2.1-or-later

import vitrio as vio

def test_returns_the_shared_read_format_selector():
	assert isinstance(
		vio.ImageFileReadFormatSelector.get_shared(),
		vio.ImageFileReadFormatSelector
	)

def test_returns_the_shared_write_format_selector():
	assert isinstance(
		vio.ImageFileWriteFormatSelector.get_shared(),
		vio.ImageFileWriteFormatSelector
	)

def test_always_returns_the_same_read_format_selector():
	first = vio.ImageFileReadFormatSelector.get_shared()
	second = vio.ImageFileReadFormatSelector.get_shared()
	assert first is second

def test_always_returns_the_same_write_format_selector():
	first = vio.ImageFileWriteFormatSelector.get_shared()
	second = vio.ImageFileWriteFormatSelector.get_shared()
	assert first is second

def test_the_shared_selectors_hold_the_bundled_formats(tmp_path):
	path = tmp_path / 'image.mrc'
	writers = vio.FileImageWriterProvider(
		vio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(
		path, vio.ImageDescriptor((4, 6), 2, vio.NumericalType.float32)
	)
	assert writers.file_count == 1
