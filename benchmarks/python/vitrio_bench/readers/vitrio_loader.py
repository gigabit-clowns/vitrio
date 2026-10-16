# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio as it is meant to be used: a loader on a thread pool, each batch
read in one call, and the next batches under way while one is used."""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader, default_workers

class VitrioLoader(Reader):
	"""
	Reads each batch with one `read_batch_async` call.

	Parameters:
		workers: Threads of the loader's pool. Defaults to what vitrio's
			loader runs when given none.
		cache: Open readers the loader keeps. 0 keeps none, which is the
			default of `vio.loader`.
		in_flight: Batches read ahead of the one being used. 0 reads each
			batch only when it is asked for.
	"""

	name = 'vitrio-loader'
	defaults = {'workers': None, 'cache': 0, 'in_flight': 2}

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

	def _start(self, rows: np.ndarray):
		images = self.new_batch(len(rows))
		locations = [self._locations[row] for row in rows]
		return images, self._vio.read_batch_async(
			self._loader, images, locations
		)

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		pending = deque()
		for rows in batches:
			pending.append(self._start(rows))
			if len(pending) > self.params['in_flight']:
				images, reading = pending.popleft()
				reading.get()
				yield images
		while pending:
			images, reading = pending.popleft()
			reading.get()
			yield images

	def close(self) -> None:
		# Destroying the loader joins its threads.
		self._loader = None
		self._locations = None
