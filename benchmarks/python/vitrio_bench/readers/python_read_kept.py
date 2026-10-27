# SPDX-License-Identifier: LGPL-2.1-or-later

"""Plain reads with Python's files, neither mapped nor through mrcfile: each
stack opened once, and only the bytes a particle or a patch needs read."""

from __future__ import annotations

from collections.abc import Iterable, Iterator
from typing import BinaryIO

import numpy as np

from ..datasets import Picks
from . import Reader
from ._mrc import layout

def _read_into(file: BinaryIO, offset: int, target: np.ndarray) -> None:
	"""
	Fill a contiguous array with the bytes of a file from an offset.

	Raises:
		EOFError: If the file ends first.
	"""
	view = memoryview(target).cast('B')
	end = offset + len(view)
	file.seek(offset)
	while view:
		count = file.readinto(view)
		if not count:
			raise EOFError(f"{file.name} ends before byte {end}.")
		view = view[count:]

class PythonReadKept(Reader):
	"""
	Opens each stack once with `open`, keeps it open, and reads with `seek`
	and `readinto`: a particle in one read, a patch one row at a time, so
	that no byte is read that the row does not need.
	"""

	name = 'python-read-kept'
	reads_patches = True

	def open(self) -> None:
		self._paths = list(self.dataset.stacks)
		self._files = {}

	def _file(self, stack: int):
		entry = self._files.get(stack)
		if entry is None:
			shape, dtype, offset = layout(self._paths[stack])
			file = open(self._paths[stack], 'rb', buffering=0)
			entry = (file, shape, dtype, offset)
			self._files[stack] = entry
		return entry

	def _particle(self, row: int) -> np.ndarray:
		file, (_, ny, nx), dtype, offset = self._file(self.stack_of[row])
		image = np.empty((ny, nx), dtype)
		_read_into(file, offset + int(self.index_of[row]) * image.nbytes, image)
		return image

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		for rows in batches:
			images = self.new_batch(len(rows))
			for position, row in enumerate(rows):
				images[position] = self._particle(row)
			yield images

	def _crop(self, image: int, y: int, x: int, side: int) -> np.ndarray:
		file, (_, ny, nx), dtype, offset = self._file(self.stack_of[image])
		patch = np.empty((side, side), dtype)
		first_row = int(self.index_of[image]) * ny + y
		for row in range(side):
			element = (first_row + row) * nx + x
			_read_into(file, offset + element * dtype.itemsize, patch[row])
		return patch

	def patches(
		self,
		batches: Iterable[np.ndarray],
		picks: Picks
	) -> Iterator[np.ndarray]:
		return self.crop_patches(batches, picks, self._crop)

	def close(self) -> None:
		for file, *_ in self._files.values():
			file.close()
		self._files = {}
