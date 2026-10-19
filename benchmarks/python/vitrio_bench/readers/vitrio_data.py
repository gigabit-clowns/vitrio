# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio as users of `vio.data` write it: a dataset of an image source, and
its DataLoader reading batches ahead."""

from __future__ import annotations

from collections.abc import Iterable, Iterator

import numpy as np

from . import Reader, default_workers
from ._vitrio import LOADER, make_loader, remove_scratch, scratch_bytes

class VitrioData(Reader):
	"""
	Goes through the batches with `vio.data.DataLoader`.

	Left at their defaults, `workers`, `cache` and `scratch` leave the
	reading to the loader `vio.data` shares across the process, as a user
	who names no loader does. Given any of them, the run makes a loader
	of its own, as `vitrio-loader` does.

	Parameters:
		prefetch: Batches the DataLoader has under way at once.
		workers, cache, scratch, run_length, budget: Those of
			`vitrio-loader`, except that `cache` defaults to what the shared
			loader keeps.
	"""

	name = 'vitrio-data'
	defaults = {**LOADER, 'cache': None, 'prefetch': 4}

	def _shared(self) -> bool:
		params = self.params
		return (
			params['workers'] is None and params['cache'] is None
			and params['scratch'] == 'none'
		)

	def open(self) -> None:
		import vitrio as vio
		import vitrio.data

		self._vio = vio
		self._scratch_bytes = None
		if self._shared():
			from vitrio.data import _default_loader

			self._loader = None
			locations = [
				vio.ImageLocation(str(self.dataset.stacks[stack]), int(index))
				for stack, index in zip(self.stack_of, self.index_of)
			]
			self.params['workers'] = default_workers()
			self.params['cache'] = _default_loader.DEFAULT_CACHE
		else:
			if self.params['cache'] is None:
				self.params['cache'] = 0
			self._loader, locations = make_loader(self)
		self._dataset = vio.data.Dataset({
			'image': vio.data.ImageSource(locations, data_type=self.dtype),
		})

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		loader = self._vio.data.DataLoader(
			self._dataset,
			batch_sampler=batches,
			loader=self._loader,
			prefetch=self.params['prefetch']
		)
		for batch in loader:
			yield batch['image']

	def close(self) -> None:
		self._dataset = None
		self._loader = None
		self._scratch_bytes = scratch_bytes(self)
		remove_scratch(self)

	def metrics(self) -> dict:
		return {'scratch_bytes': self._scratch_bytes}
