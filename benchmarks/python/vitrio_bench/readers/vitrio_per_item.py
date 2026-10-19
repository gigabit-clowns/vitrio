# SPDX-License-Identifier: LGPL-2.1-or-later

"""The ablation of `vitrio-loader`: the same loader, reading one particle per
call and waiting for each.

What sets the two apart is batching and reading ahead, and nothing else, so
the gap between them is what those two are worth."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader
from ._vitrio import LOADER, make_loader, remove_scratch

class VitrioPerItem(Reader):
	"""
	Reads each particle with a `read_batch_async` call of its own, and
	waits for it.

	It takes the parameters of `vitrio-loader`'s loader: `workers`,
	`cache`, `scratch`, `run_length` and `budget`.
	"""

	name = 'vitrio-per-item'
	defaults = LOADER

	def open(self) -> None:
		import vitrio as vio

		self._vio = vio
		self._loader, self._locations = make_loader(self)

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		read = self._vio.read_batch_async
		for rows in batches:
			images = self.new_batch(len(rows))
			for position, row in enumerate(rows):
				read(
					self._loader,
					images[position:position + 1],
					[self._locations[row]]
				).get()
			yield images

	def close(self) -> None:
		self._loader = None
		self._locations = None
		remove_scratch(self)
