# SPDX-License-Identifier: LGPL-2.1-or-later

"""numpy's memory maps: every stack mapped once, and the particles of a batch
gathered from each stack with one fancy index."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader
from ._mrc import layout

class NumpyMemmap(Reader):
	"""
	Maps each stack with `np.memmap` the first time it is read, and copies
	a batch's particles of each stack out at once.
	"""

	name = 'numpy-memmap'
	reads_patches = True

	def open(self) -> None:
		self._paths = list(self.dataset.stacks)
		self._maps = {}

	def _map(self, stack: int) -> np.memmap:
		mapped = self._maps.get(stack)
		if mapped is None:
			shape, dtype, offset = layout(self._paths[stack])
			mapped = np.memmap(
				self._paths[stack], dtype=dtype, mode='r', offset=offset,
				shape=shape
			)
			self._maps[stack] = mapped
		return mapped

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		for rows in batches:
			images = self.new_batch(len(rows))
			stacks = self.stack_of[rows]
			indices = self.index_of[rows]
			for stack in np.unique(stacks):
				positions = np.flatnonzero(stacks == stack)
				images[positions] = self._map(stack)[indices[positions]]
			yield images

	def _crop(self, image: int, y: int, x: int, side: int) -> np.ndarray:
		plane = self._map(self.stack_of[image])[self.index_of[image]]
		return plane[y:y + side, x:x + side]

	def patches(
		self,
		batches: Iterable[np.ndarray],
		picks: Picks
	) -> Iterator[np.ndarray]:
		return self.crop_patches(batches, picks, self._crop)

	def close(self) -> None:
		self._maps = {}
