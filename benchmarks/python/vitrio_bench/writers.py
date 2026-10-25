# SPDX-License-Identifier: LGPL-2.1-or-later

"""The ways a dataset is written that the procedure `write_batches`
measures: vitrio's, and the one Python programs use.

A writer writes the stacks of a dataset that has not been generated, into a
directory of the storage measured, a batch of images at a time in the order
the stacks hold them. A layout of one image per file writes each image as a
file of its own, as micrographs are.
"""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable
from pathlib import Path
from typing import Any, ClassVar

import numpy as np

from .datasets import Dataset
from .readers import Skip, default_workers

class Writer:
	"""
	Writes the stacks of a dataset.

	A subclass opens what it writes with in `open`, writes the batches in
	`write`, and makes every file complete in `close`.
	"""

	name: ClassVar[str]
	defaults: ClassVar[dict[str, Any]] = {}
	# The Python modules it needs, which it is skipped without.
	requires: ClassVar[tuple[str, ...]] = ()

	def __init__(self, dataset: Dataset, params: dict[str, Any]) -> None:
		allowed = {'name', 'dtype', *self.defaults}
		unknown = set(params) - allowed
		if unknown:
			raise ValueError(
				f"The writer {self.name} has no parameter {sorted(unknown)}. "
				f"It has {sorted(allowed - {'name'})}."
			)
		self.dataset = dataset
		self.params = {
			'dtype': 'float32',
			**self.defaults,
			**{key: value for key, value in params.items() if key != 'name'},
		}
		self.dtype = np.dtype(self.params['dtype'])
		self.single = all(size == 1 for size in dataset.sizes)

	def path_of(self, directory: Path, stack: int) -> Path:
		"""The file of a stack: an image, where each stack holds one."""
		suffix = 'mrc' if self.single else 'mrcs'
		return directory / f'stack_{stack:06d}.{suffix}'

	def open(self, directory: Path) -> None:
		"""Open what the writer writes with, into `directory`."""

	def write(self, batches: Iterable[tuple[np.ndarray, np.ndarray]]) -> None:
		"""
		Write each batch: the images, and the row of the dataset each one
		is, in the order of the stacks.
		"""
		raise NotImplementedError

	def close(self) -> None:
		"""Make every file complete, and let go of what was opened."""

	def unavailable(self) -> str | None:
		"""Why the writer cannot run here, if it cannot."""
		for module in self.requires:
			try:
				__import__(module)
			except ImportError:
				return f"{self.name} needs {module}."
		return None

	def files(self) -> list[Path]:
		"""Files the writer reads, which a cold run evicts: none."""
		return []

	def metrics(self) -> dict[str, Any]:
		"""Metrics of its own."""
		return {}

	def describe(self) -> dict[str, Any]:
		"""The writer, as a record states it."""
		return {'name': self.name, **self.params}

class VitrioSaver(Writer):
	"""
	vitrio's saver: the stacks are declared to a writer provider, and each
	batch is handed to `write_batch_async`, a few batches in flight. A stack
	is finished with `close_async` as soon as its last batch is stored, a few
	of them at once, while the next ones are written. Images that are files
	of their own are written with `write_single`, as a program writes a
	micrograph.

	Builds without `close_async` close each stack as it is complete instead,
	which waits for nothing: their writers sync no file.

	Parameters:
		workers: Threads of the saver. Defaults to vitrio's default.
		in_flight: Batches handed to the saver before the first is waited
			for.
		finishing: Stacks finishing at once, before the oldest of them is
			waited for.
	"""

	name = 'vitrio-saver'
	defaults = {'workers': None, 'in_flight': 2, 'finishing': 16}

	def open(self, directory: Path) -> None:
		import vitrio as vio
		self._vio = vio
		self._directory = directory
		self._keys = [
			str(self.path_of(directory, stack))
			for stack in range(len(self.dataset.sizes))
		]
		self._writers = None
		self._closed = set()
		if self.single:
			return
		data_type = getattr(vio.NumericalType, self.dataset.dtype.name)
		box = self.dataset.box
		self._writers = vio.writer_provider()
		for key, size in zip(self._keys, self.dataset.sizes):
			self._writers.declare(
				key, vio.ImageDescriptor((size, box, box), 2, data_type)
			)
		workers = self.params['workers'] or default_workers()
		self._saver = vio.saver(self._writers, workers=int(workers))

	def write(self, batches: Iterable[tuple[np.ndarray, np.ndarray]]) -> None:
		vio = self._vio
		stack_of, index_of = self.dataset.locate()
		if self.single:
			stored = self.dataset.dtype
			for images, rows in batches:
				for image, row in zip(images, rows):
					vio.write_single(
						image.astype(stored, copy=False),
						self._keys[stack_of[row]]
					)
			return
		left = list(self.dataset.sizes)
		finishing = deque()
		close_async = getattr(self._writers, 'close_async', None)

		def store(writing, rows: np.ndarray) -> None:
			writing.get()
			stacks, counts = np.unique(stack_of[rows], return_counts=True)
			for stack, count in zip(stacks, counts):
				left[stack] -= int(count)
				if left[stack]:
					continue
				key = self._keys[stack]
				self._closed.add(key)
				if close_async is None:
					self._writers.close(key)
					continue
				if len(finishing) == self.params['finishing']:
					finishing.popleft().get()
				finishing.append(close_async(key))

		pending = deque()
		for images, rows in batches:
			locations = [
				vio.ImageLocation(self._keys[stack_of[row]], int(index_of[row]))
				for row in rows
			]
			pending.append(
				(vio.write_batch_async(self._saver, images, locations), rows)
			)
			if len(pending) > self.params['in_flight']:
				store(*pending.popleft())
		while pending:
			store(*pending.popleft())
		while finishing:
			finishing.popleft().get()

	def close(self) -> None:
		if self._writers is not None:
			for key in self._keys:
				if key not in self._closed:
					self._writers.close(key)
		self._saver = None
		self._writers = None

class MrcfileMmap(Writer):
	"""
	mrcfile: each stack created with `mrcfile.new_mmap` and its images
	assigned to the map, a batch at a time, and each image that is a file of
	its own written with `mrcfile.new`.
	"""

	name = 'mrcfile-mmap'
	requires = ('mrcfile',)

	def open(self, directory: Path) -> None:
		import mrcfile
		self._mrcfile = mrcfile
		self._directory = directory
		self._open = {}

	def _map(self, stack: int):
		mrc = self._open.get(stack)
		if mrc is None:
			box = self.dataset.box
			mode = {'float32': 2, 'float16': 12, 'int16': 1}[
				self.dataset.dtype.name
			]
			mrc = self._mrcfile.new_mmap(
				str(self.path_of(self._directory, stack)),
				shape=(self.dataset.sizes[stack], box, box),
				mrc_mode=mode,
				overwrite=True
			)
			self._open[stack] = mrc
		return mrc

	def write(self, batches: Iterable[tuple[np.ndarray, np.ndarray]]) -> None:
		stack_of, index_of = self.dataset.locate()
		stored = self.dataset.dtype
		for images, rows in batches:
			if self.single:
				for image, row in zip(images, rows):
					self._mrcfile.new(
						str(self.path_of(self._directory, stack_of[row])),
						data=image.astype(stored, copy=False),
						overwrite=True
					).close()
				continue
			for image, row in zip(images, rows):
				self._map(stack_of[row]).data[index_of[row]] = image

	def close(self) -> None:
		for mrc in self._open.values():
			mrc.close()
		self._open = {}

WRITERS: dict[str, type[Writer]] = {
	writer.name: writer for writer in (VitrioSaver, MrcfileMmap)
}

def get(name: str) -> type[Writer]:
	"""
	The writer of a name.

	Raises:
		ValueError: If no writer has that name.
	"""
	try:
		return WRITERS[name]
	except KeyError:
		raise ValueError(
			f"There is no writer {name!r}. There are {sorted(WRITERS)}."
		) from None

__all__ = ['Skip', 'WRITERS', 'Writer', 'get']
