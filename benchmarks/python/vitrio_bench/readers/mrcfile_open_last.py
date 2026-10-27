# SPDX-License-Identifier: LGPL-2.1-or-later

"""mrcfile without a mapping: each file read whole into memory with
`mrcfile.open`, as scripts that extract particles from micrographs do."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader

class MrcfileOpenLast(Reader):
	"""
	Opens a file with `mrcfile.open`, which reads all of its values into
	memory, and keeps the last file it opened, so that the rows of one file
	in a row read it once. Copies the particles out one at a time.

	Keeping every file would hold the whole dataset in memory, which is what
	`preload` measures.
	"""

	name = 'mrcfile-open-last'
	requires = ('mrcfile',)
	reads_patches = True

	def open(self) -> None:
		import mrcfile

		self._open = mrcfile.open
		self._paths = [str(path) for path in self.dataset.stacks]
		self._stack = None
		self._mrc = None

	def _data(self, stack: int):
		if stack != self._stack:
			# The values of the last file are let go before the next is read,
			# so that two files are never held at once.
			self._close_last()
			self._mrc = self._open(self._paths[stack], mode='r', permissive=True)
			self._stack = stack
		return self._mrc.data

	def _close_last(self) -> None:
		if self._mrc is not None:
			self._mrc.close()
		self._mrc = None
		self._stack = None

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		for rows in batches:
			images = self.new_batch(len(rows))
			for position, row in enumerate(rows):
				data = self._data(self.stack_of[row])
				images[position] = (
					data[self.index_of[row]] if data.ndim == 3 else data
				)
			yield images

	def _crop(self, image: int, y: int, x: int, side: int) -> np.ndarray:
		data = self._data(self.stack_of[image])
		plane = data[self.index_of[image]] if data.ndim == 3 else data
		return plane[y:y + side, x:x + side]

	def patches(
		self,
		batches: Iterable[np.ndarray],
		picks: Picks
	) -> Iterator[np.ndarray]:
		return self.crop_patches(batches, picks, self._crop)

	def close(self) -> None:
		self._close_last()
