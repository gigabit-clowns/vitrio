# SPDX-License-Identifier: LGPL-2.1-or-later

"""Everything read into memory before the first batch: the fastest any
reader can hand batches out, and where that stops fitting."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np
import psutil

from . import Reader, Skip
from ._mrc import layout

# How much of the memory that is free a preload may take. Above it the
# machine would swap, which measures the swap.
FREE_SHARE = 0.8

class Preload(Reader):
	"""
	Reads every stack whole, sequentially, into one array, then hands
	batches out of it.
	"""

	name = 'preload'

	def open(self) -> None:
		box = self.dataset.box
		needed = self.dataset.count * box * box * self.dtype.itemsize
		available = psutil.virtual_memory().available
		if needed > FREE_SHARE * available:
			raise Skip(
				f"Preloading needs {needed} bytes, and {available} are free."
			)
		self._all = np.empty((self.dataset.count, box, box), self.dtype)
		first = 0
		for path, size in zip(self.dataset.stacks, self.dataset.sizes):
			shape, dtype, offset = layout(path)
			values = np.fromfile(
				path, dtype=dtype, count=size * box * box, offset=offset
			)
			self._all[first:first + size] = values.reshape(shape)
			first += size

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		for rows in batches:
			yield self._all[rows]

	def close(self) -> None:
		self._all = None
