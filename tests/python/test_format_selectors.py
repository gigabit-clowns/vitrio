# SPDX-License-Identifier: LGPL-2.1-or-later

import vitrio

def test_returns_the_shared_read_format_selector():
	assert isinstance(
		vitrio.ImageFileReadFormatSelector.get_shared(),
		vitrio.ImageFileReadFormatSelector
	)

def test_returns_the_shared_write_format_selector():
	assert isinstance(
		vitrio.ImageFileWriteFormatSelector.get_shared(),
		vitrio.ImageFileWriteFormatSelector
	)

def test_always_returns_the_same_read_format_selector():
	first = vitrio.ImageFileReadFormatSelector.get_shared()
	second = vitrio.ImageFileReadFormatSelector.get_shared()
	assert first is second

def test_always_returns_the_same_write_format_selector():
	first = vitrio.ImageFileWriteFormatSelector.get_shared()
	second = vitrio.ImageFileWriteFormatSelector.get_shared()
	assert first is second

def test_the_shared_selectors_hold_the_bundled_formats(tmp_path):
	path = tmp_path / 'image.mrc'
	writers = vitrio.FileImageWriterProvider(
		vitrio.ImageFileWriteFormatSelector.get_shared()
	)
	writers.declare(
		path, vitrio.ImageDescriptor((4, 6), 2, vitrio.NumericalType.float32)
	)
	assert writers.file_count == 1
