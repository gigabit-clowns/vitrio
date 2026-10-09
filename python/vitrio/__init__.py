# SPDX-License-Identifier: LGPL-2.1-or-later

"""Reads and writes the image files of electron microscopy.

Everything below is private. `_binding` mirrors the C++ API, with the same
names and the same required arguments, and the modules beside it add what
Python expects on top. This file is the whole public surface.

An array is given as whatever hands out host memory through DLPack or the
buffer protocol, and `Array`, which the reads return, hands its own out in
both ways. No memory is copied in either direction.

An asynchronous read or write returns a `Completion`, which keeps the array
it works on alive. Destroying a completion that is not ready waits for it.
"""

from __future__ import annotations

from ._binding import (
	Array as Array,
	CachingImageReaderProvider as CachingImageReaderProvider,
	Completion as Completion,
	Executor as Executor,
	ExecutorImageLoader as ExecutorImageLoader,
	ExecutorImageSaver as ExecutorImageSaver,
	FileImageReaderProvider as FileImageReaderProvider,
	FileImageWriterProvider as FileImageWriterProvider,
	ImageDescriptor as ImageDescriptor,
	ImageFileReadFormatSelector as ImageFileReadFormatSelector,
	ImageFileWriteFormatSelector as ImageFileWriteFormatSelector,
	ImageLoader as ImageLoader,
	ImageLocation as ImageLocation,
	ImageReaderProvider as ImageReaderProvider,
	ImageSaver as ImageSaver,
	ImageWriterProvider as ImageWriterProvider,
	IndexTable as IndexTable,
	NumericalType as NumericalType,
	SynchronousExecutor as SynchronousExecutor,
	ThreadPoolExecutor as ThreadPoolExecutor,
	get_core_extents as get_core_extents,
	get_library_version as _get_library_version,
	read_batch_async as read_batch_async,
	write_batch_async as write_batch_async,
)

# Imported for its effect: it teaches NumericalType the names and the numpy
# types it is also given as.
from . import _numerical_type as _numerical_type

from ._functions import (
	loader as loader,
	query_descriptor as query_descriptor,
	read as read,
	read_patches_async as read_patches_async,
	reader_provider as reader_provider,
	saver as saver,
	write as write,
	write_single as write_single,
	write_stack as write_stack,
	writer_provider as writer_provider,
)

__version__ = '.'.join(str(part) for part in _get_library_version())
