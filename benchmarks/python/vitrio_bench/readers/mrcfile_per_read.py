# SPDX-License-Identifier: LGPL-2.1-or-later

"""mrcfile as cryoSTAR and DynaMight use it: the stack mapped anew for every
particle, which is read and copied out."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader

class MrcfilePerRead(Reader):
	"""
	Opens the stack of each particle with `mrcfile.mmap`, copies the
	particle out, and closes it.
	"""

	name = 'mrcfile-per-read'
	requires = ('mrcfile',)

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
