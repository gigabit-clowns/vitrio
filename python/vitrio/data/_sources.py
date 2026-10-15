# SPDX-License-Identifier: LGPL-2.1-or-later

"""Where the values of a field of a dataset come from.

A source holds a value for each row of a dataset, all of one shape and one
data type, and reads the values of any rows into an array. The images of a
dataset are a source, and so are patches cropped out of larger images and
arrays already in memory.
"""

from __future__ import annotations

import os
from collections import deque
from collections.abc import Sequence
from typing import Any, Protocol

import numpy as np

from .._binding import (
	Completion,
	ImageLoader,
	ImageLocation,
	ImageReaderProvider,
	IndexTable,
	NumericalType,
	get_core_extents,
	read_batch_async,
)
from .._functions import query_descriptor, read_patches_async, reader_provider

# The numerical types numpy names differently, and the one it has no type for.
_NUMPY_NAMES = {
	NumericalType.boolean: 'bool',
	NumericalType.complex_float32: 'complex64',
	NumericalType.complex_float64: 'complex128',
}

def numpy_dtype(data_type: object) -> np.dtype:
	"""
	Get the numpy type of a data type stated in any way `NumericalType` takes.

	Raises:
		ValueError: If numpy has no such type, as for `complex_float16`.
	"""
	if isinstance(data_type, np.dtype):
		return data_type
	data_type = NumericalType(data_type)
	if data_type == NumericalType.complex_float16:
		raise ValueError("numpy has no complex number of 16-bit halves.")
	return np.dtype(_NUMPY_NAMES.get(data_type, data_type.name))

class Pending(Protocol):
	"""What a source hands back for a read that is under way."""

	@property
	def is_ready(self) -> bool: ...

	def get(self) -> None: ...

class Source(Protocol):
	"""
	The values of one field of a dataset, a value for each row.

	`shape` and `dtype` are those of the value of one row, so the values of
	`n` rows are read into an array of shape `(n, *shape)`.
	"""

	@property
	def shape(self) -> tuple[int, ...]:
		"""The shape of the value of one row."""
		...

	@property
	def dtype(self) -> np.dtype:
		"""The data type of the values."""
		...

	def __len__(self) -> int: ...

	def read_async(
		self,
		loader: ImageLoader,
		rows: np.ndarray,
		out: np.ndarray
	) -> list[Pending]:
		"""
		Start reading the values of some rows.

		Args:
			loader: Where reads of files are dispatched.
			rows: The rows to read, as integers within the length of the
				source.
			out: Where the value of each row lands, in the order of `rows`.
				Its shape is `(len(rows), *shape)` and its type `dtype`.

		Returns:
			What to wait for before `out` holds the values. It may be
			empty, when the values are in `out` already.
		"""
		...

	def subset(self, rows: np.ndarray) -> Source:
		"""Get the source of only these rows, in this order."""
		...

def _to_location(value: Any) -> ImageLocation:
	if isinstance(value, ImageLocation):
		return value
	if isinstance(value, bytes):
		value = value.decode()
	if isinstance(value, (str, os.PathLike)):
		return ImageLocation(value)
	raise TypeError(
		f"A {type(value).__name__} is not an image location. A location is "
		"an ImageLocation, or the key of a whole file."
	)

def _as_location_array(locations: Any) -> np.ndarray:
	if hasattr(locations, 'to_numpy'):
		locations = locations.to_numpy()
	array = np.empty(np.shape(locations), dtype=object)
	array[...] = locations
	if array.ndim not in (1, 2):
		raise ValueError(
			"The locations are a location for each row, or a row of "
			f"locations for each row. They have {array.ndim} dimensions."
		)
	flat = array.reshape(-1)
	for position, value in enumerate(flat):
		flat[position] = _to_location(value)
	return flat.reshape(array.shape)

def _image_shape(
	location: ImageLocation,
	readers: ImageReaderProvider | None
) -> tuple[tuple[int, ...], NumericalType, int | None]:
	"""
	Get the shape of what a location names, the type its file holds, and how
	many images or volumes its file stacks, or None when it is not a stack.
	"""
	descriptor = query_descriptor(location.key, readers)
	is_stack = len(descriptor.extents) > descriptor.core_rank
	stacked = descriptor.extents[0] if is_stack else None
	if not location.has_index_in_stack:
		return tuple(descriptor.extents), descriptor.data_type, stacked
	if not is_stack:
		raise ValueError(
			f"{location} names an image of a stack, and {location.key} is "
			"not a stack. Name it without an index."
		)
	return (
		tuple(get_core_extents(descriptor)), descriptor.data_type, stacked
	)

class ImageSource:
	"""
	Images or volumes read from files, a location for each row.

	A row may have several locations instead, as many for every row: the
	tilts of a particle, the halves of a movie, an image and its target.
	Its value is then those images stacked, and `shape` has the number of
	locations of a row in front of the shape of one image.

	Every location has to name an image of the same shape. The files are
	asked for their shape when the source is made, each file once, and a
	location that does not fit is reported then.
	"""

	def __init__(
		self,
		locations: Sequence[Any] | np.ndarray,
		data_type: object | None = None,
		readers: ImageReaderProvider | None = None,
		check: bool = True
	) -> None:
		"""
		Args:
			locations: The location of the image of each row, or a row of
				locations for each row: `ImageLocation` objects, or keys of whole
				files.
			data_type: The type to read the values as. Defaults to the one
				the first file holds.
			readers: Where files are asked for their shape. Defaults to a
				provider that opens the file at the path a key is.
			check: Whether to ask every file for its shape, and every stack
				whether it holds the indices asked of it. Otherwise only the
				first file is asked.

		Raises:
			ValueError: If two locations name images of different shapes,
				an index is past the end of its stack, or there are no
				locations.
		"""
		self._locations = _as_location_array(locations)
		if self._locations.size == 0:
			raise ValueError("A source of images needs a location.")

		core, file_type = self._describe(readers, check)
		self._core = core
		self.dtype = numpy_dtype(
			file_type if data_type is None else data_type
		)
		self.shape = tuple(self._locations.shape[1:]) + core

	def _describe(
		self,
		readers: ImageReaderProvider | None,
		check: bool
	) -> tuple[tuple[int, ...], NumericalType]:
		if readers is None:
			readers = reader_provider()
		flat = self._locations.reshape(-1)
		first = flat[0]
		core, file_type, _ = _image_shape(first, readers)
		if not check:
			return core, file_type

		# The last index asked of each file, or -1 when it is asked whole.
		last: dict[tuple[str, bool], ImageLocation] = {}
		for location in flat:
			group = (location.key, location.has_index_in_stack)
			seen = last.get(group)
			if seen is None or (
				location.has_index_in_stack
				and location.index_in_stack > seen.index_in_stack
			):
				last[group] = location

		for location in last.values():
			shape, _, stacked = _image_shape(location, readers)
			if shape != core:
				raise ValueError(
					f"{location} is of shape {shape}, and {first} of shape "
					f"{core}. The images of a source are all of one shape."
				)
			if (
				location.has_index_in_stack
				and location.index_in_stack >= stacked
			):
				raise ValueError(
					f"{location} is past the end of {location.key}, which "
					f"stacks {stacked}."
				)
		return core, file_type

	@property
	def locations(self) -> np.ndarray:
		"""The location of each row, as an array of `ImageLocation` objects."""
		return self._locations

	def __len__(self) -> int:
		return len(self._locations)

	def read_async(
		self,
		loader: ImageLoader,
		rows: np.ndarray,
		out: np.ndarray
	) -> list[Pending]:
		"""Start reading the values of some rows, as `Source.read_async`."""
		locations = self._locations[rows]
		if locations.ndim == 1:
			return [read_batch_async(loader, out, list(locations))]

		# All the images of a batch go in one read when its rows lie one
		# after the other, which they do in an array of its own.
		if out.flags.c_contiguous:
			flat = out.reshape((-1, *self._core))
			return [read_batch_async(loader, flat, list(locations.ravel()))]
		return [
			read_batch_async(loader, out[:, column], list(locations[:, column]))
			for column in range(locations.shape[1])
		]

	def subset(self, rows: np.ndarray) -> ImageSource:
		"""Get the source of only these rows, in this order."""
		subset = object.__new__(ImageSource)
		subset._locations = self._locations[rows]
		subset._core = self._core
		subset.dtype = self.dtype
		subset.shape = self.shape
		return subset

class _Scatter:
	"""A read into an array of its own, moved to its rows once done."""

	def __init__(
		self,
		completion: Completion,
		read: np.ndarray,
		out: np.ndarray,
		positions: np.ndarray
	) -> None:
		self._completion = completion
		self._read = read
		self._out = out
		self._positions = positions
		self._moved = False

	@property
	def is_ready(self) -> bool:
		return self._completion.is_ready

	def get(self) -> None:
		self._completion.get()
		if not self._moved:
			self._out[self._positions] = self._read
			self._moved = True

class PatchSource:
	"""
	Patches cropped out of larger images, a patch for each row.

	Each row names the image its patch is cropped from and the centre of
	the patch, which lands at index `extent // 2` within it, as RELION has
	it. A patch reaching past the edge of its image is read as far as the
	image goes, and the rest of it is `fill`. The rows of a batch that share
	an image are read in one call.

	Particles picked on micrographs are patches, and so are the boxes of a
	tomogram around its picked objects, or around random centres.
	"""

	def __init__(
		self,
		images: Sequence[Any] | np.ndarray,
		centres: Any,
		shape: Sequence[int],
		data_type: object | None = None,
		fill: float = 0,
		readers: ImageReaderProvider | None = None,
		check: bool = True
	) -> None:
		"""
		Args:
			images: The image each row is cropped from: an `ImageLocation`
				or the key of a file that holds one image or volume.
			centres: The centre of each patch, a row of integers for each
				row, slowest axis first, as `(y, x)` or `(z, y, x)`.
			shape: The shape of one patch.
			data_type: The type to read the values as. Defaults to the one
				the first image holds.
			fill: The value of what a patch has beyond its image.
			readers: Where files are asked for their shape. Defaults to a
				provider that opens the file at the path a key is.
			check: Whether to ask every image for its shape. Otherwise only
				the first is asked.

		Raises:
			ValueError: If the images, centres and patches are not of one
				rank, there are not as many centres as images, a centre is
				negative, or there are no rows.
		"""
		locations = _as_location_array(images)
		if locations.ndim != 1:
			raise ValueError("A patch is cropped out of one image.")
		if locations.size == 0:
			raise ValueError("A source of patches needs a row.")

		self.shape = tuple(int(extent) for extent in shape)
		self._centres = np.asarray(centres, dtype=np.int64)
		rank = len(self.shape)
		if self._centres.shape != (len(locations), rank):
			raise ValueError(
				f"The centres are of shape {self._centres.shape}, and are "
				f"a centre of rank {rank} for each of {len(locations)} rows."
			)
		if (self._centres < 0).any():
			raise ValueError("A centre is an index, and is not negative.")

		codes: dict[ImageLocation, int] = {}
		self._codes = np.fromiter(
			(codes.setdefault(location, len(codes)) for location in locations),
			dtype=np.intp,
			count=len(locations)
		)
		self._images = list(codes)

		if readers is None:
			readers = reader_provider()
		file_type = None
		for image in self._images if check else self._images[:1]:
			extents, image_type, _ = _image_shape(image, readers)
			file_type = file_type or image_type
			if len(extents) != rank:
				raise ValueError(
					f"{image} is of shape {extents}, and the patches are of "
					f"rank {rank}. Name one image of a stack by its index."
				)
		self.dtype = numpy_dtype(
			file_type if data_type is None else data_type
		)
		self.fill = fill

	@property
	def images(self) -> list[ImageLocation]:
		"""The images the patches are cropped from, each once."""
		return self._images

	@property
	def centres(self) -> np.ndarray:
		"""The centre of the patch of each row."""
		return self._centres

	def __len__(self) -> int:
		return len(self._codes)

	def read_async(
		self,
		loader: ImageLoader,
		rows: np.ndarray,
		out: np.ndarray
	) -> list[Pending]:
		"""Start reading the values of some rows, as `Source.read_async`."""
		out[...] = self.fill
		codes = self._codes[rows]
		order = np.argsort(codes, kind='stable')
		starts = np.flatnonzero(np.diff(codes[order], prepend=-1))
		ends = np.append(starts[1:], len(order))

		pending: list[Pending] = []
		for start, end in zip(starts, ends):
			positions = order[start:end]
			image = self._images[codes[positions[0]]]
			centres = IndexTable.from_array(self._centres[rows[positions]])
			first, last = positions[0], positions[-1]
			if last - first + 1 == len(positions):
				pending.append(read_patches_async(
					loader, out[first:last + 1], image, centres
				))
				continue

			read = np.full((len(positions), *self.shape), self.fill, out.dtype)
			completion = read_patches_async(loader, read, image, centres)
			pending.append(_Scatter(completion, read, out, positions))
		return pending

	def subset(self, rows: np.ndarray) -> PatchSource:
		"""Get the source of only these rows, in this order."""
		subset = object.__new__(PatchSource)
		subset.shape = self.shape
		subset.dtype = self.dtype
		subset.fill = self.fill
		subset._centres = self._centres[rows]
		subset._codes = self._codes[rows]
		subset._images = self._images
		return subset

class ArraySource:
	"""
	Values already in memory, the first axis of an array running along the
	rows.
	"""

	def __init__(self, array: Any) -> None:
		"""
		Args:
			array: The values. Anything numpy takes as an array, which it
				uses without a copy where it can.
		"""
		self._array = np.asarray(array)
		if self._array.ndim == 0:
			raise ValueError("An array of values has an axis of rows.")
		self.shape = self._array.shape[1:]
		self.dtype = self._array.dtype

	@property
	def array(self) -> np.ndarray:
		"""The values."""
		return self._array

	def __len__(self) -> int:
		return len(self._array)

	def read_async(
		self,
		loader: ImageLoader,
		rows: np.ndarray,
		out: np.ndarray
	) -> list[Pending]:
		"""Start reading the values of some rows, as `Source.read_async`."""
		np.take(self._array, rows, axis=0, out=out)
		return []

	def subset(self, rows: np.ndarray) -> ArraySource:
		"""Get the source of only these rows, in this order."""
		return ArraySource(self._array[rows])

def wait_all(pending: Sequence[Pending]) -> None:
	"""
	Wait for every read, then raise the first error any of them had.

	Each is waited for even after one fails, since the arrays they read
	into must not be handed out while a read still writes to them.
	"""
	error = None
	for read in pending:
		try:
			read.get()
		except Exception as raised:  # noqa: BLE001
			error = error or raised
	if error is not None:
		raise error

def preload(
	source: Source,
	loader: ImageLoader,
	chunk: int = 1024,
	in_flight: int = 4
) -> ArraySource:
	"""
	Read every value of a source into memory.

	Args:
		source: What to read.
		loader: Where reads of files are dispatched.
		chunk: How many rows to read in one call.
		in_flight: How many calls to keep under way at once.

	Returns:
		ArraySource: The values, in an array of `len(source)` rows.
	"""
	values = np.empty((len(source), *source.shape), source.dtype)
	# A read left behind by an error is waited for once it is dropped.
	window: deque[list[Pending]] = deque()
	for start in range(0, len(source), chunk):
		stop = min(start + chunk, len(source))
		rows = np.arange(start, stop)
		window.append(source.read_async(loader, rows, values[start:stop]))
		if len(window) == in_flight:
			wait_all(window.popleft())
	while window:
		wait_all(window.popleft())
	return ArraySource(values)
