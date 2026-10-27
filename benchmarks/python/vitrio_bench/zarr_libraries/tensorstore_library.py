# SPDX-License-Identifier: LGPL-2.1-or-later

"""tensorstore: a store written with one future and read with another, and
patches read with a future each, every one of them under way at once, as
its users do. tensorstore writes arrays alone, so the group of the store is
written beside it, as a document.

Parameters:
	concurrency: The limit of its file I/O and of its copies and codecs at
		once, `file_io_concurrency` and `data_copy_concurrency`. Defaults
		to tensorstore's, which follow the machine.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np

from . import StoreLayout, ZarrLibrary, patch_slices, write_group

class TensorstoreLibrary(ZarrLibrary):
	name = 'tensorstore'
	requires = ('tensorstore',)
	package = 'tensorstore'
	defaults: dict[str, Any] = {'concurrency': None}

	def _context(self):
		import tensorstore as ts

		limit = self.params['concurrency']
		if limit is None:
			return ts.Context()
		return ts.Context({
			'file_io_concurrency': {'limit': int(limit)},
			'data_copy_concurrency': {'limit': int(limit)},
		})

	def _open(self, store: Path, **options):
		import tensorstore as ts

		spec = {
			'driver': 'zarr3',
			'kvstore': {'driver': 'file', 'path': str(store / '0')},
			**options,
		}
		return ts.open(spec, context=self._context()).result()

	def write(
		self,
		store: Path,
		values: np.ndarray,
		layout: StoreLayout
	) -> None:
		write_group(store, layout)
		metadata = layout.array_metadata()
		for key in ('zarr_format', 'node_type'):
			metadata.pop(key)
		array = self._open(store, metadata=metadata, create=True)
		array.write(values).result()

	def read(self, store: Path, out: np.ndarray) -> None:
		out[...] = self._open(store, open=True).read().result()

	def read_patches(
		self,
		store: Path,
		layout: StoreLayout,
		corners: np.ndarray,
		out: np.ndarray
	) -> None:
		array = self._open(store, open=True)
		extents = out.shape[1:]
		futures = [
			array[patch_slices(corner, extents)].read() for corner in corners
		]
		for slot, future in enumerate(futures):
			out[slot] = future.result()
