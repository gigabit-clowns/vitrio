# SPDX-License-Identifier: LGPL-2.1-or-later

"""numpy's memory maps: every stack mapped once, and the particles of a batch
gathered from each stack with one fancy index."""

from __future__ import annotations

import mmap
from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader, Skip
from ._mrc import layout

ADVICE = {'normal': None, 'random': getattr(mmap, 'MADV_RANDOM', None)}

class NumpyMemmapKept(Reader):
	"""
	Maps each stack with `np.memmap` the first time it is read, and copies
	a batch's particles of each stack out at once.

	Parameters:
		advice: `normal` leaves the kernel to read around each fault, as it
			does for any map. `random` maps with `MADV_RANDOM`, so that a
			fault reads its page and nothing around it.
	"""

	name = 'numpy-memmap-kept'
	defaults = {'advice': 'normal'}
	reads_patches = True

	def open(self) -> None:
		advice = self.params['advice']
		if advice not in ADVICE:
			raise ValueError(f"advice is one of {sorted(ADVICE)}, not {advice!r}.")
		if advice != 'normal' and ADVICE[advice] is None:
			raise Skip(f"This system has no MADV_{advice.upper()}.")
		self._advice = ADVICE[advice]
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
			if self._advice is not None:
				# numpy keeps the map it reads through as `_mmap`.
				mapped._mmap.madvise(self._advice)
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
