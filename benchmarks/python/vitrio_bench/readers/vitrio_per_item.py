# SPDX-License-Identifier: LGPL-2.1-or-later

"""The ablation of `vitrio-loader`: the same loader, reading one particle per
call and waiting for each.

What sets the two apart is batching and reading ahead, and nothing else, so
the gap between them is what those two are worth."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader, default_workers

class VitrioPerItem(Reader):
	"""
	Reads each particle with a `read_batch_async` call of its own, and
	waits for it.

	Parameters:
		workers: Threads of the loader's pool, as `vitrio-loader` takes
			them.
		cache: Open readers the loader keeps, as `vitrio-loader` takes
			them.
	"""

	name = 'vitrio-per-item'
	defaults = {'workers': None, 'cache': 0}

	def open(self) -> None:
		import vitrio as vio

		if self.params['workers'] is None:
			self.params['workers'] = default_workers()
		self._vio = vio
		self._loader = vio.loader(
			workers=self.params['workers'],
			cache=self.params['cache'] or None
		)
		stacks = [str(path) for path in self.dataset.stacks]
		self._locations = [
			vio.ImageLocation(stacks[stack], int(index))
			for stack, index in zip(self.stack_of, self.index_of)
		]

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
