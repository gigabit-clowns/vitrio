# SPDX-License-Identifier: LGPL-2.1-or-later

"""mrcfile as cryoSTAR and DynaMight use it: the stack mapped anew for every
particle, which is read and copied out."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader

class MrcfileMmapPerRead(Reader):
	"""
	Opens the stack of each particle with `mrcfile.mmap`, copies the
	particle out, and closes it.
	"""

	name = 'mrcfile-mmap-per-read'
	requires = ('mrcfile',)
	reads_patches = True

	def open(self) -> None:
		import mrcfile

		self._mmap = mrcfile.mmap
		self._paths = [str(path) for path in self.dataset.stacks]

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		for rows in batches:
			images = self.new_batch(len(rows))
			for position, row in enumerate(rows):
				path = self._paths[self.stack_of[row]]
				with self._mmap(path, mode='r', permissive=True) as mrc:
					data = mrc.data
					# A stack of one particle reads as an image.
					images[position] = (
						data[self.index_of[row]] if data.ndim == 3 else data
					)
			yield images

	def _crop(self, image: int, y: int, x: int, side: int) -> np.ndarray:
		path = self._paths[self.stack_of[image]]
		with self._mmap(path, mode='r', permissive=True) as mrc:
			data = mrc.data
			plane = data[self.index_of[image]] if data.ndim == 3 else data
			return np.array(plane[y:y + side, x:x + side])

	def patches(
		self,
		batches: Iterable[np.ndarray],
		picks: Picks
	) -> Iterator[np.ndarray]:
		return self.crop_patches(batches, picks, self._crop)
