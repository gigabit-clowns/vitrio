# SPDX-License-Identifier: LGPL-2.1-or-later

"""mrcfile with every stack kept open: the best a loop over particles can do
without changing how it reads."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader

class MrcfileOpen(Reader):
	"""
	Maps each stack with `mrcfile.mmap` the first time it is read, keeps it
	mapped, and copies the particles out one at a time.
	"""

	name = 'mrcfile-open'
	requires = ('mrcfile',)
	reads_patches = True

	def open(self) -> None:
		import mrcfile

		self._mmap = mrcfile.mmap
		self._paths = [str(path) for path in self.dataset.stacks]
		self._open = {}

	def _data(self, stack: int):
		mrc = self._open.get(stack)
		if mrc is None:
			mrc = self._mmap(self._paths[stack], mode='r', permissive=True)
			self._open[stack] = mrc
		return mrc.data

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
		for mrc in self._open.values():
			mrc.close()
		self._open = {}
