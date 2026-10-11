# SPDX-License-Identifier: LGPL-2.1-or-later

"""A dataset: named fields, a value of each for every row."""

from __future__ import annotations

import operator
from collections.abc import Callable, Mapping
from typing import Any

import numpy as np

from .._binding import ImageLoader
from ._default_loader import resolve_loader
from ._sources import ArraySource, Pending, Source, preload, wait_all
from ._tables import check_lengths

Allocator = Callable[[tuple[int, ...], np.dtype], np.ndarray]

def _is_source(value: object) -> bool:
	return hasattr(value, 'read_async') and hasattr(value, 'subset')

def normalize_rows(rows: Any, length: int) -> np.ndarray:
	"""
	Turn what names rows into an array of their indices.

	Takes integers, negative ones counting from the end, or a mask of one
	boolean for each row.

	Raises:
		IndexError: If a row is past either end.
		TypeError: If `rows` holds neither integers nor booleans.
	"""
	rows = np.asarray(rows)
	if rows.ndim != 1:
		raise IndexError("Rows are named by an array of one dimension.")
	if rows.dtype == np.bool_:
		if len(rows) != length:
			raise IndexError(
				f"A mask of {len(rows)} booleans names rows of {length}."
			)
		return np.flatnonzero(rows)
	if rows.size == 0:
		return np.empty(0, dtype=np.intp)
	if not np.issubdtype(rows.dtype, np.integer):
		raise TypeError(
			f"Rows are named by integers, not by values of {rows.dtype}."
		)
	rows = rows.astype(np.intp)
	if rows.min() < -length or rows.max() >= length:
		raise IndexError(f"A row is past the end of {length} rows.")
	return np.where(rows < 0, rows + length, rows)

class PendingBatch:
	"""A batch under way. Its arrays are handed out once it is complete."""

	def __init__(
		self,
		values: dict[str, np.ndarray],
		pending: list[Pending]
	) -> None:
		self._values = values
		self._pending = pending

	@property
	def is_ready(self) -> bool:
		"""Whether every read of the batch is done."""
		return all(read.is_ready for read in self._pending)

	def get(self) -> dict[str, np.ndarray]:
		"""
		Wait for the batch.

		Returns:
			dict[str, np.ndarray]: The value of each field for each row of
			the batch, in the order its rows were asked for.

		Raises:
			Whatever a read of the batch raised. Every read is waited for
			first.
		"""
		wait_all(self._pending)
		self._pending = []
		return self._values

class Dataset:
	"""
	Rows of named fields: images, patches, and the metadata that goes with
	them.

	A field is a source, such as an `ImageSource`, or a column, which is
	anything numpy takes as an array of a value for each row: an array of
	CTF parameters, a column of a DataFrame. Every field has a value for
	each row.

	Indexing a dataset with an integer reads one row, and with a slice or
	an array of integers reads a batch of rows into an array for each field.
	Each batch also holds the index of each of its rows, under `index`. So
	it works as it is with the DataLoader of PyTorch, which reads a batch at
	a time when its sampler draws batches:

		torch.utils.data.DataLoader(
			dataset,
			sampler=torch.utils.data.BatchSampler(
				torch.utils.data.RandomSampler(dataset), 64, False
			),
			batch_size=None
		)

	A dataset only describes its rows, and holds nothing that reads them.
	Each read is given the loader it goes through, a thread pool with its
	open readers and maybe a scratch, and a read given none goes through a
	loader shared by the process. Indexing always does. `DataLoader` of
	`vitrio.data` takes a loader of its own, and needs no worker process.
	"""

	def __init__(
		self,
		fields: Mapping[str, Any],
		index: str | None = 'index'
	) -> None:
		"""
		Args:
			fields: The fields by name, each a source or a column.
			index: The name the indices of the rows of a batch go under.
				None leaves them out.

		Raises:
			ValueError: If there are no fields, they are not all as long,
				or one has the name `index` is given.
		"""
		if not fields:
			raise ValueError("A dataset needs a field.")
		if index is not None and index in fields:
			raise ValueError(
				f"The field {index!r} would hide the indices of the rows. "
				"Name the indices otherwise with `index`."
			)

		self._fields = {
			str(name): value if _is_source(value) else np.asarray(value)
			for name, value in fields.items()
		}
		for name, value in self._fields.items():
			if not _is_source(value) and value.ndim == 0:
				raise ValueError(
					f"The field {name!r} is a single value, and not a value "
					"for each row."
				)
		self._length = check_lengths(self._fields)
		self._index = index

	def _derive(self, fields: dict[str, Any]) -> Dataset:
		return Dataset(fields, self._index)

	@property
	def fields(self) -> Mapping[str, Source | np.ndarray]:
		"""The fields by name."""
		return dict(self._fields)

	@property
	def index(self) -> str | None:
		"""The name the indices of the rows go under, if anywhere."""
		return self._index

	def __len__(self) -> int:
		return self._length

	def read_async(
		self,
		rows: Any,
		allocator: Allocator | None = None,
		loader: ImageLoader | None = None
	) -> PendingBatch:
		"""
		Start reading a batch of rows.

		Args:
			rows: The rows, as integers, negative ones counting from the end,
				or as a mask of a boolean for each row. A row may be asked
				for more than once.
			allocator: What makes the array of each field, given its shape
				and its numpy type. Defaults to `numpy.empty`. An allocator
				of pinned memory lets PyTorch copy the batch to a GPU
				without staging it.
			loader: Where reads of files are dispatched. Defaults to the
				loader shared by the process.

		Returns:
			PendingBatch: The batch, once it is complete.
		"""
		rows = normalize_rows(rows, self._length)
		loader = resolve_loader(loader)
		allocate = np.empty if allocator is None else allocator
		values: dict[str, np.ndarray] = {}
		pending: list[Pending] = []
		for name, field in self._fields.items():
			if _is_source(field):
				out = allocate((len(rows), *field.shape), field.dtype)
				pending.extend(field.read_async(loader, rows, out))
			else:
				out = allocate((len(rows), *field.shape[1:]), field.dtype)
				np.take(field, rows, axis=0, out=out)
			values[name] = out

		if self._index is not None:
			out = allocate((len(rows),), np.dtype(np.int64))
			out[...] = rows
			values[self._index] = out
		return PendingBatch(values, pending)

	def __getitem__(self, key: Any) -> dict[str, Any]:
		"""
		Read one row, or a batch of rows.

		Args:
			key: An integer for one row, or a slice, an array of integers or
				a mask for a batch.

		Returns:
			dict[str, Any]: The value of each field. For a batch, an array
			for each field whose first axis runs along the rows.
		"""
		try:
			row = operator.index(key)
		except TypeError:
			pass
		else:
			batch = self.read_async([row]).get()
			return {name: values[0] for name, values in batch.items()}

		if isinstance(key, slice):
			key = np.arange(*key.indices(self._length))
		return self.read_async(key).get()

	def subset(self, rows: Any) -> Dataset:
		"""
		Get a dataset of only some of the rows, in the order asked for.

		It shares the sources of this one. Its indices count its own rows.

		Args:
			rows: The rows, named as `read_async` takes them.
		"""
		rows = normalize_rows(rows, self._length)
		return self._derive({
			name: field.subset(rows) if _is_source(field) else field[rows]
			for name, field in self._fields.items()
		})

	def preload(
		self,
		names: Any = None,
		loader: ImageLoader | None = None
	) -> Dataset:
		"""
		Get a dataset whose sources have all been read into memory.

		Going through it afterwards reads no file.

		Args:
			names: The fields to read. Defaults to every source not already
				in memory.
			loader: Where reads of files are dispatched. Defaults to the
				loader shared by the process.
		"""
		loader = resolve_loader(loader)
		if names is None:
			names = [
				name for name, field in self._fields.items()
				if _is_source(field) and not isinstance(field, ArraySource)
			]
		fields = dict(self._fields)
		for name in names:
			if not _is_source(fields[name]):
				raise ValueError(f"The field {name!r} is not a source.")
			fields[name] = preload(fields[name], loader)
		return self._derive(fields)
