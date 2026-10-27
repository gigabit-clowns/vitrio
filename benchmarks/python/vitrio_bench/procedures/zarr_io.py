# SPDX-License-Identifier: LGPL-2.1-or-later

"""How fast a ZARR store is written and read, by vitrio and by the
libraries Python reads ZARR with.

The values of a dataset of one stack are written as a store, as a volume or
as a stack of images, then read back whole, then read as patches at corners
drawn at random: cubes of a volume, or squares of the images of a stack.
Each is done `repeats` times, and its median and best are reported. Every
library writes the store vitrio writes, its chunks and codecs the same (see
`vitrio_bench.zarr_libraries`), so that they do the same work. The stores
are written beside the dataset, on the storage being measured, and removed
after.

The "reader" of this procedure is the library, `--reader.name`: `vitrio`,
`zarr-python` or `tensorstore`, with its parameters.

Workload:
	kind: `volume`, a volume of the dataset's particles as its slices, or
		`stack`, a stack of them as images.
	chunk_edge: The side of a chunk, of a cube for a volume and of a square
		of an image for a stack. Defaults to vitrio's.
	repeats: How many times each step is done.
	patch: The side of a patch.
	patches: How many patches are read.
	seed: What decides the corners of the patches.
	cold: How the store's pages are dropped from the cache before each
		read: `none`, `fadvise` or `drop`, as `--environment.cold` drops
		the dataset's.
	sync: Whether a write lasts until what it wrote is on the storage, as
		`os.sync` makes it. vitrio syncs the files it writes; the others
		leave it to the system.
	verify: Whether to check that what is read is what was written, and
		hash it, to check the libraries against each other. It is done
		outside what is timed.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import statistics
import time
import uuid
from pathlib import Path
from typing import Any

import numpy as np

from .. import cache
from ..datasets import Dataset
from ..metrics import max_rss_bytes
from ..zarr_libraries import StoreLayout, ZarrLibrary, get

WORKLOAD: dict[str, Any] = {
	'kind': 'volume',
	'chunk_edge': None,
	'repeats': 3,
	'patch': 64,
	'patches': 100,
	'seed': 0,
	'cold': 'none',
	'sync': True,
	'verify': False,
}
KINDS = ('volume', 'stack')

# vitrio's chunks, VITRIO_DEFAULT_ZARR_*_CHUNK_EDGE, and the variables of
# the environment that override them.
CHUNK_EDGES = {'volume': 128, 'stack': 1024}
CHUNK_VARIABLES = {
	'volume': 'VITRIO_ZARR_VOLUME_CHUNK_EDGE',
	'stack': 'VITRIO_ZARR_IMAGE_CHUNK_EDGE',
}

def make_reader(
	dataset: Dataset,
	params: dict[str, Any],
	scratchdir: str | Path | None
) -> ZarrLibrary:
	"""The library, with the parameters a run gives it."""
	return get(params.get('name', 'vitrio'))(params)

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out, with the chunks it writes.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot, or
			the dataset is not one stack.
	"""
	unknown = set(params) - set(WORKLOAD)
	if unknown:
		raise ValueError(
			f"zarr has no workload parameter {sorted(unknown)}. It has "
			f"{sorted(WORKLOAD)}."
		)
	if len(dataset.sizes) != 1:
		raise ValueError("zarr writes the values of a dataset of one stack.")
	workload = {**WORKLOAD, **params}
	if workload['kind'] not in KINDS:
		raise ValueError(f"A store is written as one of {KINDS}.")
	edge = workload['chunk_edge']
	edge = CHUNK_EDGES[workload['kind']] if edge is None else int(edge)
	for name in ('repeats', 'patch', 'patches'):
		workload[name] = int(workload[name])
		if workload[name] < 1:
			raise ValueError(f"zarr takes at least one of {name}.")
	if edge < 1:
		raise ValueError("A chunk is at least one wide.")
	if workload['cold'] not in cache.MODES:
		raise ValueError(f"A store is made cold by one of {cache.MODES}.")
	workload['chunk_edge'] = edge
	layout = _layout(dataset, workload)
	workload['chunks'] = list(layout.chunks)
	workload['patch_extents'] = list(_patch_extents(layout, workload['patch']))
	return workload

def _layout(dataset: Dataset, workload: dict[str, Any]) -> StoreLayout:
	# As vitrio chooses its chunks: a chunk is one image of a stack deep.
	shape = (dataset.count, dataset.box, dataset.box)
	edge = workload['chunk_edge']
	core_rank = 3 if workload['kind'] == 'volume' else 2
	stack = len(shape) - core_rank
	chunks = tuple(
		1 if axis < stack else min(extent, edge)
		for axis, extent in enumerate(shape)
	)
	return StoreLayout(shape, dataset.dtype, chunks, core_rank)

def _patch_extents(layout: StoreLayout, patch: int) -> tuple[int, ...]:
	stack = len(layout.shape) - layout.core_rank
	return tuple(
		1 if axis < stack else min(extent, patch)
		for axis, extent in enumerate(layout.shape)
	)

def _corners(
	layout: StoreLayout,
	extents: tuple[int, ...],
	count: int,
	seed: int
) -> np.ndarray:
	rng = np.random.default_rng([seed, 3])
	highest = np.asarray(layout.shape) - np.asarray(extents) + 1
	corners = rng.integers(0, highest, size=(count, len(layout.shape)))
	# In the order of the images they lie in, as a reader takes them.
	order = np.lexsort(corners.T[::-1])
	return np.ascontiguousarray(corners[order])

def _files(store: Path) -> list[Path]:
	return [path for path in store.rglob('*') if path.is_file()]

def _check_chunks(store: Path, layout: StoreLayout, library: str) -> None:
	metadata = json.loads((store / '0' / 'zarr.json').read_text())
	written = metadata['chunk_grid']['configuration']['chunk_shape']
	if tuple(written) != layout.chunks:
		raise ValueError(
			f"{library} wrote chunks of {written}, not {list(layout.chunks)}."
		)

def _hash(values: np.ndarray) -> str:
	return hashlib.sha256(np.ascontiguousarray(values).tobytes()).hexdigest()

def _summary(name: str, times: list[float], nbytes: int) -> dict[str, Any]:
	median = statistics.median(times)
	return {
		f'{name}_s': median,
		f'{name}_best_s': min(times),
		f'{name}_bytes_per_s': nbytes / median if median > 0 else None,
	}

def run(
	dataset: Dataset,
	reader: ZarrLibrary,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Write a store, read it whole and in patches, and measure each."""
	import vitrio as vio

	layout = _layout(dataset, workload)
	# Before vitrio first writes a store, which reads it once.
	os.environ[CHUNK_VARIABLES[workload['kind']]] = str(workload['chunk_edge'])
	values = np.asarray(vio.read(dataset.stacks[0])).reshape(layout.shape)
	extents = _patch_extents(layout, workload['patch'])
	corners = _corners(layout, extents, workload['patches'], workload['seed'])

	root = dataset.path.parent / f'zarr-{uuid.uuid4().hex}'
	store = root / 'store.zarr'
	metrics: dict[str, Any] = {'data_bytes': values.nbytes}
	try:
		times = []
		for _ in range(workload['repeats']):
			if root.exists():
				shutil.rmtree(root)
			root.mkdir(parents=True)
			begun = time.perf_counter()
			reader.write(store, values, layout)
			if workload['sync'] and hasattr(os, 'sync'):
				os.sync()
			times.append(time.perf_counter() - begun)
		_check_chunks(store, layout, reader.name)
		stored = sum(path.stat().st_size for path in _files(store))
		metrics.update({
			**_summary('write', times, values.nbytes),
			'stored_bytes': stored,
			'compression_ratio': values.nbytes / stored if stored else None,
			'files': len(_files(store)),
		})

		read = np.empty_like(values)
		times = []
		for _ in range(workload['repeats']):
			cache.evict(_files(store), workload['cold'])
			begun = time.perf_counter()
			reader.read(store, read)
			times.append(time.perf_counter() - begun)
		metrics.update(_summary('read', times, values.nbytes))
		if workload['verify']:
			if not np.array_equal(read, values):
				raise ValueError(f"{reader.name} read what was not written.")
			metrics['read_hash'] = _hash(read)
		del read

		patches = np.empty((len(corners), *extents), dtype=values.dtype)
		times = []
		for _ in range(workload['repeats']):
			cache.evict(_files(store), workload['cold'])
			begun = time.perf_counter()
			reader.read_patches(store, layout, corners, patches)
			times.append(time.perf_counter() - begun)
		metrics.update(_summary('patches', times, patches.nbytes))
		metrics['patches_per_s'] = (
			len(corners) / metrics['patches_s']
			if metrics['patches_s'] > 0 else None
		)
		if workload['verify']:
			for slot, corner in enumerate(corners):
				region = tuple(
					slice(int(start), int(start) + extent)
					for start, extent in zip(corner, extents)
				)
				if not np.array_equal(patches[slot], values[region]):
					raise ValueError(
						f"{reader.name} read a patch at {corner.tolist()} that "
						"is not what was written."
					)
			metrics['patch_hash'] = _hash(patches)
	finally:
		shutil.rmtree(root, ignore_errors=True)

	metrics['peak_memory_bytes'] = max_rss_bytes()
	return metrics
