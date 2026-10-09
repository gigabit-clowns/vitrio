# SPDX-License-Identifier: LGPL-2.1-or-later

"""Reading and writing image files, with the collaborators defaulted.

Wraps `vitrio._binding`: the same functions, but the formats default to the
bundled ones, the reader provider to one that opens files with them, and a
data type is taken in any of the ways `NumericalType` takes one. A
collaborator that takes a default comes after the arguments that do not.

An array is whatever hands out host memory through DLPack or the buffer
protocol: a numpy array, a tensor of PyTorch or JAX, a `memoryview`, a
`vitrio.Array`. Its memory is read or written where it is and never copied.
Memory on a device is refused, and so is read-only memory where it would be
written.
"""

from __future__ import annotations

import os
from collections.abc import Sequence
from typing import Any

from . import _binding
from ._binding import (
	Array,
	CachingImageReaderProvider,
	Completion,
	Executor,
	ExecutorImageLoader,
	ExecutorImageSaver,
	FileImageReaderProvider,
	FileImageWriterProvider,
	ImageDescriptor,
	ImageFileReadFormatSelector,
	ImageFileWriteFormatSelector,
	ImageLoader,
	ImageLocation,
	ImageReaderProvider,
	ImageWriterProvider,
	IndexTable,
	NumericalType,
	ThreadPoolExecutor,
)

HostArray = Any
Key = str | os.PathLike

def _default_worker_count() -> int:
	return os.cpu_count() or 1

def _resolve_executor(
	executor: Executor | None,
	workers: int | None
) -> Executor:
	if executor is None:
		executor = ThreadPoolExecutor(
			workers if workers is not None else _default_worker_count()
		)
	return executor

def _resolve_read_formats(
	formats: ImageFileReadFormatSelector | None
) -> ImageFileReadFormatSelector:
	if formats is None:
		formats = ImageFileReadFormatSelector.get_shared()
	return formats

def _resolve_write_formats(
	formats: ImageFileWriteFormatSelector | None
) -> ImageFileWriteFormatSelector:
	if formats is None:
		formats = ImageFileWriteFormatSelector.get_shared()
	return formats

def _resolve_readers(
	readers: ImageReaderProvider | None
) -> ImageReaderProvider:
	if readers is None:
		readers = reader_provider()
	return readers

def _resolve_data_type(data_type: object | None) -> NumericalType | None:
	if data_type is None:
		return None
	return NumericalType(data_type)

def reader_provider(
	cache: int | None = None,
	formats: ImageFileReadFormatSelector | None = None
) -> ImageReaderProvider:
	"""
	Assemble a reader provider that opens files.

	Args:
		cache: How many open readers to keep between reads. Defaults to
			opening a file every time it is asked for, which suits reads
			that do not revisit a file.
		formats: The formats to recognize files with. Defaults to the
			bundled ones.

	Returns:
		ImageReaderProvider: The assembled provider. Its keys are paths.
	"""
	readers = FileImageReaderProvider(_resolve_read_formats(formats))
	if cache is not None:
		readers = CachingImageReaderProvider(readers, cache)
	return readers

def loader(
	workers: int | None = None,
	cache: int | None = None,
	formats: ImageFileReadFormatSelector | None = None,
	executor: Executor | None = None
) -> ExecutorImageLoader:
	"""
	Assemble an image loader over a thread pool.

	Wires the four objects a loader stands on: the formats, a reader
	provider, an executor and the loader itself. A caller reaches
	`read_batch_async` and `read_patches_async` without naming them.

	Each loader built this way owns its executor. Two of them are two
	thread pools, each sized to the machine. Pass one executor to both to
	avoid that.

	Args:
		workers: How many threads to run reads on. Defaults to what the
			machine reports. Ignored when `executor` is given.
		cache: How many open readers to keep between reads. Defaults to
			opening a file every time it is asked for.
		formats: The formats to recognize files with. Defaults to the
			bundled ones.
		executor: Where reads run. Defaults to a thread pool of its own.

	Returns:
		ExecutorImageLoader: The assembled loader.
	"""
	return ExecutorImageLoader(
		reader_provider(cache, formats),
		_resolve_executor(executor, workers)
	)

def writer_provider(
	formats: ImageFileWriteFormatSelector | None = None
) -> FileImageWriterProvider:
	"""
	Assemble a writer provider that creates files.

	A file has to be declared on it, with the descriptor it is created
	as, before anything can be written to it. It is closed once it is
	finished.

	Args:
		formats: The formats to create files with. Defaults to the bundled
			ones.

	Returns:
		FileImageWriterProvider: The assembled provider, with no file
		declared. Its keys are paths.
	"""
	return FileImageWriterProvider(_resolve_write_formats(formats))

def saver(
	writers: ImageWriterProvider,
	workers: int | None = None,
	executor: Executor | None = None
) -> ExecutorImageSaver:
	"""
	Assemble an image saver over a thread pool.

	The provider is taken and not assembled, since whoever writes keeps it
	to declare the files and to close them.

	Each saver built this way owns its executor, as each loader built by
	`loader` does.

	Args:
		writers: Where a key becomes an open writer.
		workers: How many threads to run writes on. Defaults to what the
			machine reports. Ignored when `executor` is given.
		executor: Where writes run. Defaults to a thread pool of its own.

	Returns:
		ExecutorImageSaver: The assembled saver.
	"""
	return ExecutorImageSaver(writers, _resolve_executor(executor, workers))

def query_descriptor(
	key: Key,
	readers: ImageReaderProvider | None = None
) -> ImageDescriptor:
	"""
	Get the shape and data type of what an image file holds.

	The extents are those of the whole file, slowest axis first, so a
	stack reports the axis it stacks along before the shape of one of
	its images. `get_core_extents` leaves that axis out, which is the
	shape a batch destination carries beside its leading extent:

		descriptor = query_descriptor(path)
		destination = np.empty(
			(len(locations), *get_core_extents(descriptor)),
			dtype=np.float32
		)

	Args:
		key: The file to ask about, as its key in `readers`.
		readers: Where the file becomes a reader. Defaults to a provider
			that opens the file at the path `key` is.

	Returns:
		ImageDescriptor: The descriptor of the file.
	"""
	return _binding.query_descriptor(_resolve_readers(readers), key)

def read(
	location: Key | ImageLocation,
	readers: ImageReaderProvider | None = None,
	data_type: object | None = None,
	out: HostArray | None = None
) -> Array | HostArray:
	"""
	Read an image file, or one image or volume of a stack.

	Args:
		location: The file to read, as its key in `readers`, or an
			`ImageLocation` naming one image or volume of it. A string is
			a key and only a key: read `"3@stack.mrc"` by handing it to
			`ImageLocation.from_string` first.
		readers: Where the file becomes a reader. Defaults to a provider
			that opens the file at the path the key is.
		data_type: The type to read the values as. Defaults to the one
			the file holds. Cannot be given with `out`, whose own type is
			the one the values are read as.
		out: The array to read into. Its shape has to be that of what
			`location` names. Defaults to a new `Array`.

	Returns:
		`out` when it is given. Otherwise an `Array` over memory of its
		own, which numpy and whatever takes DLPack use without a copy.

	Raises:
		ValueError: If `data_type` and `out` are both given.
	"""
	readers = _resolve_readers(readers)
	if out is None:
		return _binding.read(location, readers, _resolve_data_type(data_type))

	if data_type is not None:
		raise ValueError(
			"The values are read as the data type of out, so data_type "
			"cannot be given with it."
		)

	if not isinstance(location, ImageLocation):
		location = ImageLocation(location)

	_binding.read(out, location, readers)
	return out

def read_patches_async(
	loader: ImageLoader,
	destination: HostArray,
	location: ImageLocation,
	centres: IndexTable | HostArray | Sequence[Sequence[int]]
) -> Completion:
	"""
	Crop a batch of equally sized patches out of one image.

	Returns before the reads are done. Each slot of `destination`
	receives the patch around one centre, which lands at index
	`extent // 2` within it. A patch reaching past the edge of the image
	is read as far as the image goes, and the rest of its slot keeps
	what `destination` held beforehand.

	Args:
		loader: Where the reads are dispatched.
		destination: Where the patches land. Its leading extent is the
			batch size and the rest are the shape of one patch.
		location: The image every patch is cropped from.
		centres: The centre of each patch: an `IndexTable`, an array of
			integers with one row per centre, or any sequence of
			sequences. Each centre has the rank of one patch.

	Returns:
		Completion: Ready once every patch has been read or has failed.
		It keeps `destination` alive, and destroying it waits for the
		reads.
	"""
	if isinstance(centres, Sequence):
		table = IndexTable(max(len(destination.shape) - 1, 0))
		for centre in centres:
			table.add(centre)
		centres = table
	elif not isinstance(centres, IndexTable):
		centres = IndexTable.from_array(centres)
	return _binding.read_patches_async(loader, destination, location, centres)

def write_single(
	array: HostArray,
	path: Key,
	formats: ImageFileWriteFormatSelector | None = None,
	data_type: object | None = None
) -> None:
	"""
	Write an array to an image file as one image or volume.

	Args:
		array: The values to write.
		path: The file to write them to.
		formats: The formats to choose from. Defaults to the bundled ones.
		data_type: The type to store the values as. Defaults to the one
			the array already carries.
	"""
	_binding.write_single(
		array,
		path,
		_resolve_write_formats(formats),
		_resolve_data_type(data_type)
	)

def write_stack(
	array: HostArray,
	path: Key,
	formats: ImageFileWriteFormatSelector | None = None,
	data_type: object | None = None
) -> None:
	"""
	Write an array to an image file as a stack of images or volumes.

	The leading extent of the array is the axis the file stacks along
	and the rest are the shape of one image or volume.

	Args:
		array: The values to write.
		path: The file to write them to.
		formats: The formats to choose from. Defaults to the bundled ones.
		data_type: The type to store the values as. Defaults to the one
			the array already carries.
	"""
	_binding.write_stack(
		array,
		path,
		_resolve_write_formats(formats),
		_resolve_data_type(data_type)
	)

def write(
	array: HostArray,
	path: Key,
	descriptor: ImageDescriptor,
	formats: ImageFileWriteFormatSelector | None = None
) -> None:
	"""
	Write an array to an image file as a descriptor states.

	The core rank of the descriptor is what makes the file a stack and
	not a single image or volume, and its data type is the one the file
	stores.

	Args:
		array: The values to write. Its shape has to be the extents of
			`descriptor`.
		path: The file to write them to.
		descriptor: What the file holds.
		formats: The formats to choose from. Defaults to the bundled ones.
	"""
	_binding.write(array, path, _resolve_write_formats(formats), descriptor)
