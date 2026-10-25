# SPDX-License-Identifier: LGPL-2.1-or-later

"""The libraries the procedure `zarr` writes and reads ZARR stores with:
vitrio's, and zarr-python and tensorstore as baselines.

Every library writes the same store, as vitrio writes it: an image of
OME-Zarr 0.5 whose one level, the array `0`, is of ZARR version 3, its
chunks as vitrio chooses them, encoded as bytes little endian and
compressed by blosc with zstd at level 5, the bytes shuffled. So the writes
do the same work, and the reads read the same bytes.

Each library writes and reads as its users do: vitrio with one call, which
carries out the chunks at once; zarr-python with its arrays, one patch at a
time; tensorstore with futures, every patch at once. A library names the
modules it needs in `requires` and its parameters, with their defaults, in
`defaults`. One whose modules are missing is not run, and its run is
recorded as skipped.
"""

from __future__ import annotations

import importlib
import importlib.metadata
import importlib.util
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, ClassVar

import numpy as np

#: The level of zstd, and the shuffle of blosc, vitrio writes with.
CLEVEL = 5
SHUFFLE = 'shuffle'

@dataclass(frozen=True)
class StoreLayout:
	"""
	What a store holds and how: its shape and data type, its chunks, and
	how many of its trailing axes are one image or volume.
	"""

	shape: tuple[int, ...]
	dtype: np.dtype
	chunks: tuple[int, ...]
	core_rank: int

	@property
	def dimension_names(self) -> list[str]:
		"""The names vitrio gives the axes: n for the stack, z, y, x."""
		stack = len(self.shape) - self.core_rank
		return ['n'] * stack + ['z', 'y', 'x'][3 - self.core_rank:]

	def array_metadata(self) -> dict[str, Any]:
		"""The metadata of the array, as zarr.json states it."""
		return {
			'zarr_format': 3,
			'node_type': 'array',
			'shape': list(self.shape),
			'data_type': self.dtype.name,
			'chunk_grid': {
				'name': 'regular',
				'configuration': {'chunk_shape': list(self.chunks)},
			},
			'chunk_key_encoding': {
				'name': 'default', 'configuration': {'separator': '/'},
			},
			'fill_value': 0,
			'codecs': [
				{'name': 'bytes', 'configuration': {'endian': 'little'}},
				{'name': 'blosc', 'configuration': {
					'cname': 'zstd', 'clevel': CLEVEL, 'shuffle': SHUFFLE,
					'typesize': self.dtype.itemsize, 'blocksize': 0,
				}},
			],
			'dimension_names': self.dimension_names,
		}

	def group_metadata(self) -> dict[str, Any]:
		"""The metadata of the group, an image of OME-Zarr 0.5."""
		stack = len(self.shape) - self.core_rank
		axes = [
			{'name': name, **({'type': 'space'} if axis >= stack else {})}
			for axis, name in enumerate(self.dimension_names)
		]
		return {
			'zarr_format': 3,
			'node_type': 'group',
			'attributes': {'ome': {
				'version': '0.5',
				'multiscales': [{
					'axes': axes,
					'datasets': [{
						'path': '0',
						'coordinateTransformations': [{
							'type': 'scale',
							'scale': [1.0] * len(self.shape),
						}],
					}],
				}],
			}},
		}

def write_group(store: Path, layout: StoreLayout) -> None:
	"""Write the zarr.json of the group of a store, for the libraries that
	write arrays alone."""
	store.mkdir(parents=True, exist_ok=True)
	(store / 'zarr.json').write_text(json.dumps(layout.group_metadata()))

class ZarrLibrary:
	"""
	Writes and reads ZARR stores.

	A subclass writes a whole array in `write`, and reads one whole, or
	patches of one, into arrays it is given.
	"""

	name: ClassVar[str]
	requires: ClassVar[tuple[str, ...]] = ()
	#: The distribution whose version a record states.
	package: ClassVar[str] = ''
	defaults: ClassVar[dict[str, Any]] = {}

	def __init__(self, params: dict[str, Any]) -> None:
		allowed = {'name', *self.defaults}
		unknown = set(params) - allowed
		if unknown:
			raise ValueError(
				f"The library {self.name} has no parameter {sorted(unknown)}. "
				f"It has {sorted(self.defaults)}."
			)
		self.params = {
			**self.defaults,
			**{key: value for key, value in params.items() if key != 'name'},
		}

	def unavailable(self) -> str | None:
		"""Why the library cannot run here, or None if it can."""
		needed = missing(type(self))
		if needed:
			return f"{self.name} needs {needed}."
		return None

	def files(self) -> list[Path]:
		"""It keeps no copy of the dataset, whose pages a cold run drops."""
		return []

	def describe(self) -> dict[str, Any]:
		"""The library and its settings, as a record states them."""
		version = None
		if self.package:
			try:
				version = importlib.metadata.version(self.package)
			except importlib.metadata.PackageNotFoundError:
				# Imported from a build tree, which has no metadata.
				if self.unavailable() is None:
					module = importlib.import_module(self.package)
					version = getattr(module, '__version__', None)
		return {'name': self.name, 'version': version, **self.params}

	def write(
		self,
		store: Path,
		values: np.ndarray,
		layout: StoreLayout
	) -> None:
		"""Write a whole array as a new store, which nothing is at."""
		raise NotImplementedError

	def read(self, store: Path, out: np.ndarray) -> None:
		"""Read the whole array of a store into `out`."""
		raise NotImplementedError

	def read_patches(
		self,
		store: Path,
		layout: StoreLayout,
		corners: np.ndarray,
		out: np.ndarray
	) -> None:
		"""
		Read patches of the array of a store into `out`, one per row of
		`corners`, each of the shape of a slot of `out`: one image deep
		along the axes of a stack. The corners are in the order of the
		images they lie in.
		"""
		raise NotImplementedError

def missing(library: type[ZarrLibrary]) -> list[str]:
	"""The modules a library needs that cannot be imported here."""
	return [
		module for module in library.requires
		if importlib.util.find_spec(module) is None
	]

def patch_slices(corner: np.ndarray, extents: tuple[int, ...]) -> tuple:
	"""The slices of a patch at a corner."""
	return tuple(
		slice(int(start), int(start) + extent)
		for start, extent in zip(corner, extents)
	)

from .tensorstore_library import TensorstoreLibrary    # noqa: E402
from .vitrio_library import VitrioLibrary    # noqa: E402
from .zarr_python_library import ZarrPythonLibrary    # noqa: E402

LIBRARIES: dict[str, type[ZarrLibrary]] = {
	library.name: library
	for library in (VitrioLibrary, ZarrPythonLibrary, TensorstoreLibrary)
}

def get(name: str) -> type[ZarrLibrary]:
	"""
	The library of a name.

	Raises:
		ValueError: If no library has that name.
	"""
	try:
		return LIBRARIES[name]
	except KeyError:
		raise ValueError(
			f"There is no ZARR library {name!r}. There are "
			f"{sorted(LIBRARIES)}."
		) from None
