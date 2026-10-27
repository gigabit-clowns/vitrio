# SPDX-License-Identifier: LGPL-2.1-or-later

"""zarr-python: a store written by assigning a whole array to it, and read
by indexing it, one patch at a time, as its users do. Its chunks are
encoded and moved by its own pool of threads and loop of asyncio.

Parameters:
	concurrency: The chunks it has under way at once, its
		`async.concurrency`. Defaults to zarr-python's.
	threads: The threads it encodes and decodes on, its
		`threading.max_workers`. Defaults to zarr-python's.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

import numpy as np

from . import CLEVEL, SHUFFLE, StoreLayout, ZarrLibrary, patch_slices

class ZarrPythonLibrary(ZarrLibrary):
	name = 'zarr-python'
	requires = ('zarr', 'numcodecs')
	package = 'zarr'
	defaults: dict[str, Any] = {'concurrency': None, 'threads': None}

	def _configure(self):
		import zarr

		settings = {}
		if self.params['concurrency'] is not None:
			settings['async.concurrency'] = int(self.params['concurrency'])
		if self.params['threads'] is not None:
			settings['threading.max_workers'] = int(self.params['threads'])
		if settings:
			zarr.config.set(settings)
		return zarr

	def write(
		self,
		store: Path,
		values: np.ndarray,
		layout: StoreLayout
	) -> None:
		zarr = self._configure()
		from zarr.codecs import BloscCodec, BytesCodec

		group = zarr.open_group(str(store), mode='w', zarr_format=3)
		group.attrs.update(layout.group_metadata()['attributes'])
		array = group.create_array(
			'0', shape=layout.shape, chunks=layout.chunks, dtype=layout.dtype,
			serializer=BytesCodec(endian='little'),
			compressors=[BloscCodec(
				cname='zstd', clevel=CLEVEL, shuffle=SHUFFLE,
				typesize=layout.dtype.itemsize
			)],
			chunk_key_encoding={'name': 'default', 'separator': '/'},
			fill_value=0, dimension_names=layout.dimension_names,
		)
		array[...] = values

	def _open(self, store: Path):
		zarr = self._configure()
		return zarr.open_array(str(store / '0'), mode='r')

	def read(self, store: Path, out: np.ndarray) -> None:
		out[...] = self._open(store)[...]

	def read_patches(
		self,
		store: Path,
		layout: StoreLayout,
		corners: np.ndarray,
		out: np.ndarray
	) -> None:
		array = self._open(store)
		extents = out.shape[1:]
		for slot, corner in enumerate(corners):
			out[slot] = array[patch_slices(corner, extents)]
