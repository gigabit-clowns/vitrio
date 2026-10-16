# SPDX-License-Identifier: LGPL-2.1-or-later

"""Convert first: every particle written once into an LMDB database, then
read from it, as cryodata does for Cryo-IEF and many deep learning
pipelines do with LMDB, WebDataset or FFCV.

The particles are stored as the stacks hold them, without the resizing and
quantisation cryodata does for its model, so that the comparison is of where
they are read from and not of how many bytes they take."""

from __future__ import annotations

import shutil
import time
from collections.abc import Iterable, Iterator
from pathlib import Path

import numpy as np

from . import Reader
from ._mrc import layout

# Particles written in one transaction while converting.
CHUNK = 1024
# The marker of a conversion that ran to the end.
COMPLETE = 'complete'

def _key(row: int) -> bytes:
	# Big endian, so that keys in order are rows in order and are appended.
	return int(row).to_bytes(8, 'big')

class LmdbStore(Reader):
	"""
	Converts the dataset into an LMDB database in the scratch directory,
	then reads each particle of a batch from it.

	It opens the database as readers of LMDB for deep learning do, such as
	torchvision's LSUN: read-only, without locks, and by default without
	readahead.

	Parameters:
		reuse: Whether to read a conversion an earlier run left, as a
			rerun does, rather than converting again, as a first run does.
			A run that finds no conversion converts all the same, and its
			`converted` metric says so.
		readahead: Whether the kernel may read ahead of the pages a read
			touches. Off, as LSUN has it, a database that is not cached is
			read a page at a time.
	"""

	name = 'lmdb'
	requires = ('lmdb',)
	defaults = {'reuse': False, 'readahead': False}

	def _path(self) -> Path:
		return self.require_scratchdir() / (
			f'vitrio-bench-{self.dataset.name}.lmdb'
		)

	def files(self) -> list[Path]:
		path = self._path() / 'data.mdb'
		return [path] if self.params['reuse'] and path.exists() else []

	def _convert(self, path: Path) -> None:
		import lmdb

		shutil.rmtree(path, ignore_errors=True)
		size = self.dataset.total_bytes
		env = lmdb.open(
			str(path), map_size=2 * size + (1 << 30), readahead=False,
			meminit=False
		)
		row = 0
		try:
			for stack in self.dataset.stacks:
				shape, dtype, offset = layout(stack)
				values = np.memmap(
					stack, dtype=dtype, mode='r', offset=offset, shape=shape
				)
				for first in range(0, shape[0], CHUNK):
					with env.begin(write=True) as txn:
						for image in values[first:first + CHUNK]:
							txn.put(
								_key(row),
								np.ascontiguousarray(image, self.dataset.dtype),
								append=True
							)
							row += 1
			env.sync()
		finally:
			env.close()
		(path / COMPLETE).touch()

	def open(self) -> None:
		import lmdb

		path = self._path()
		self._converted = (
			not self.params['reuse'] or not (path / COMPLETE).exists()
		)
		begin = time.perf_counter()
		if self._converted:
			self._convert(path)
		self._convert_s = time.perf_counter() - begin
		self._bytes = (path / 'data.mdb').stat().st_blocks * 512
		self._env = lmdb.open(
			str(path), readonly=True, lock=False,
			readahead=bool(self.params['readahead']), meminit=False
		)
		self._shape = (self.dataset.box, self.dataset.box)

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		stored = self.dataset.dtype
		with self._env.begin(buffers=True) as txn:
			for rows in batches:
				images = self.new_batch(len(rows))
				for position, row in enumerate(rows):
					value = np.frombuffer(txn.get(_key(row)), stored)
					images[position] = value.reshape(self._shape)
				yield images

	def close(self) -> None:
		self._env.close()

	def metrics(self) -> dict:
		return {
			'converted': self._converted,
			'convert_s': self._convert_s if self._converted else 0.0,
			'converted_bytes': self._bytes,
		}
