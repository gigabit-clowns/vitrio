# SPDX-License-Identifier: LGPL-2.1-or-later

"""numpy's memory maps: every stack mapped once, and the particles of a batch
gathered from each stack with one fancy index."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader
from ._mrc import layout

class NumpyMemmap(Reader):
	"""
	Maps each stack with `np.memmap` the first time it is read, and copies
	a batch's particles of each stack out at once.
	"""

	name = 'numpy-memmap'

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

	def close(self) -> None:
		self._maps = {}
