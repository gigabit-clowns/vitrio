# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio as it is meant to be used: a loader on a thread pool, each batch
read in one call, and the next batches under way while one is used."""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader
from ._vitrio import LOADER, make_loader, remove_scratch, scratch_bytes

class VitrioLoader(Reader):
	"""
	Reads each batch with one `read_batch_async` call.

	Parameters:
		workers: Threads of the loader's pool. Defaults to what vitrio's
			loader runs when given none.
		cache: Open readers the loader keeps. 0 keeps none, which is the
			default of `vio.loader`.
		scratch: Where the loader keeps copies of the particles it has
			read: `none`, `host` memory, a `mapped` file, or a file of a
			`directory` kept within a budget. The last two are kept in the
			run's scratch directory.
		run_length: Particles of a file the scratch takes in together.
		budget: Bytes a directory scratch may take. Defaults to what the
			dataset takes, and a margin.
		in_flight: Batches read ahead of the one being used. 0 reads each
			batch only when it is asked for.
	"""

	name = 'vitrio-loader'
	defaults = {**LOADER, 'in_flight': 2}

	def open(self) -> None:
		import vitrio as vio

		self._vio = vio
		self._loader, self._locations = make_loader(self)
		self._scratch_bytes = None

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
		# Destroying the loader joins its threads and releases its scratch.
		self._loader = None
		self._locations = None
		self._scratch_bytes = scratch_bytes(self)
		remove_scratch(self)

	def metrics(self) -> dict:
		return {'scratch_bytes': self._scratch_bytes}
