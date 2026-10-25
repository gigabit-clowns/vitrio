# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio: a store written with one call, and read with one, its chunks
carried out at once on the threads of a pool of transfers. Patches are
read through a loader, those of an image in one call, every call under way
at once.

Parameters:
	workers: The threads of the pool of transfers and of the loader.
		Defaults to the pool the process shares, and to the loader's
		default.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np

from . import StoreLayout, ZarrLibrary

class VitrioLibrary(ZarrLibrary):
	name = 'vitrio'
	requires = ('vitrio',)
	package = 'vitrio'
	defaults: dict[str, Any] = {'workers': None}

	def __init__(self, params: dict[str, Any]) -> None:
		super().__init__(params)
		self._transfers = None

	def _pool(self):
		import vitrio as vio

		if self._transfers is None and self.params['workers'] is not None:
			self._transfers = vio.FileTransferPool(
				vio.ThreadPoolExecutor(int(self.params['workers']))
			)
		return self._transfers

	def write(
		self,
		store: Path,
		values: np.ndarray,
		layout: StoreLayout
	) -> None:
		import vitrio as vio

		descriptor = vio.ImageDescriptor(
			layout.shape, layout.core_rank, vio.NumericalType(layout.dtype.name)
		)
		vio.write(values, store, descriptor, transfers=self._pool())

	def read(self, store: Path, out: np.ndarray) -> None:
		import vitrio as vio

		readers = vio.reader_provider(transfers=self._pool())
		vio.read(store, readers, out=out)

	def read_patches(
		self,
		store: Path,
		layout: StoreLayout,
		corners: np.ndarray,
		out: np.ndarray
	) -> None:
		import vitrio as vio

		loader = vio.loader(self.params['workers'], transfers=self._pool())
		extents = out.shape[1:]
		stack = len(layout.shape) - layout.core_rank
		pending = []
		if stack == 1:
			# The patches of each image, cropped out of it in one call.
			core = extents[1:]
			images = corners[:, 0]
			starts = np.flatnonzero(np.diff(images, prepend=-1))
			ends = [*starts[1:], len(images)]
			for start, end in zip(starts, ends):
				location = vio.ImageLocation(store, int(images[start]))
				centres = corners[start:end, 1:] + np.asarray(core) // 2
				slots = out[start:end].reshape(end - start, *core)
				pending.append(vio.read_patches_async(
					loader, slots, location, np.ascontiguousarray(centres)
				))
		else:
			# A volume, or an array of no stack, cropped in one call.
			centres = corners + np.asarray(extents) // 2
			pending.append(vio.read_patches_async(
				loader, out, vio.ImageLocation(store),
				np.ascontiguousarray(centres)
			))
		for completion in pending:
			completion.get()
