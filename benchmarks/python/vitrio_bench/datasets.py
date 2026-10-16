# SPDX-License-Identifier: LGPL-2.1-or-later

"""Datasets made for the benchmarks: their spec, their name, and writing them.

A dataset is described by a spec: how many particles, their box, the data
type they are stored as, the format, and how they are laid out in files.
The spec's hash names the dataset, so that the same spec makes the same
files on any machine and results from two machines can be compared.

For a format that stores values as they are, what they are does not change
what reading them costs. What does is how many files there are, how large,
and in what order they are read. So the particles are pseudo-random noise.
Never zeros or a constant: some file systems compress or deduplicate, and
a sparse file reads at the speed of memory.

Layouts:

- `single-stack`: every particle in one stack, as cryoDRGN and many
  pipelines keep them.
- `per-micrograph:<mean>`: one stack per micrograph, as RELION extracts
  them, with a number of particles in each drawn around a mean.
"""

from __future__ import annotations

import contextlib
import hashlib
import json
import os
import shutil
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

try:
	import fcntl
except ImportError:
	fcntl = None

DEFAULTS = {
	'count': 20000,
	'box': 128,
	'dtype': 'float32',
	'format': 'mrcs',
	'layout': 'single-stack',
	'seed': 0,
}
DTYPES = ('float32', 'float16', 'int16')
FORMATS = ('mrcs',)
DESCRIPTION = 'dataset.json'

# How much of a stack is generated and written at once.
CHUNK_BYTES = 1 << 28

class Missing(FileNotFoundError):
	"""A dataset was asked for that has not been generated."""

def make_spec(params: dict[str, Any]) -> dict[str, Any]:
	"""
	A complete spec: the parameters given, and the defaults for the rest.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	unknown = set(params) - set(DEFAULTS)
	if unknown:
		raise ValueError(
			f"A dataset has no parameter {sorted(unknown)}. It has "
			f"{sorted(DEFAULTS)}."
		)
	spec = {**DEFAULTS, **params}
	spec['count'] = int(spec['count'])
	spec['box'] = int(spec['box'])
	spec['seed'] = int(spec['seed'])
	if spec['count'] < 1 or spec['box'] < 1:
		raise ValueError("A dataset has at least one particle of one pixel.")
	if spec['dtype'] not in DTYPES:
		raise ValueError(f"A dataset is stored as one of {DTYPES}.")
	if spec['format'] not in FORMATS:
		raise ValueError(f"A dataset is written in one of {FORMATS}.")
	stack_sizes(spec)    # checks the layout
	return spec

def name(spec: dict[str, Any]) -> str:
	"""The name of a dataset: a hash of its spec."""
	text = json.dumps(spec, sort_keys=True)
	return hashlib.sha256(text.encode()).hexdigest()[:12]

def stack_sizes(spec: dict[str, Any]) -> list[int]:
	"""How many particles each stack of a dataset holds."""
	layout, _, argument = spec['layout'].partition(':')
	if layout == 'single-stack' and not argument:
		return [spec['count']]
	if layout == 'per-micrograph' and argument:
		mean = float(argument)
		if mean < 1:
			raise ValueError("A micrograph holds at least one particle.")
		rng = np.random.default_rng([spec['seed'], 1])
		sizes = []
		left = spec['count']
		while left > 0:
			size = int(min(max(1, rng.poisson(mean)), left))
			sizes.append(size)
			left -= size
		return sizes
	raise ValueError(
		f"A layout is 'single-stack' or 'per-micrograph:<mean>', not "
		f"{spec['layout']!r}."
	)

@dataclass
class Dataset:
	"""A dataset that has been generated, and where its particles are."""

	path: Path
	spec: dict[str, Any]
	sizes: list[int]

	@property
	def name(self) -> str:
		"""The dataset's name."""
		return self.path.name

	@property
	def count(self) -> int:
		"""How many particles it holds."""
		return self.spec['count']

	@property
	def box(self) -> int:
		"""The side of a particle, in pixels."""
		return self.spec['box']

	@property
	def dtype(self) -> np.dtype:
		"""The data type the particles are stored as."""
		return np.dtype(self.spec['dtype'])

	@property
	def stacks(self) -> list[Path]:
		"""Its files, in order."""
		return [self.path / _stack_name(i) for i in range(len(self.sizes))]

	@property
	def image_bytes(self) -> int:
		"""The bytes one particle takes in its file."""
		return self.box * self.box * self.dtype.itemsize

	@property
	def total_bytes(self) -> int:
		"""The bytes every particle takes in the files."""
		return self.count * self.image_bytes

	def locate(self) -> tuple[np.ndarray, np.ndarray]:
		"""
		Where each particle is: the stack that holds it, and its index in
		that stack. Particles are numbered stack after stack.
		"""
		sizes = np.asarray(self.sizes)
		stack = np.repeat(np.arange(len(sizes)), sizes)
		starts = np.concatenate(([0], np.cumsum(sizes)[:-1]))
		index = np.arange(self.count) - np.repeat(starts, sizes)
		return stack, index

	def describe(self) -> dict[str, Any]:
		"""The dataset, as a record states it."""
		return {
			**self.spec,
			'name': self.name,
			'path': str(self.path),
			'stacks': len(self.sizes),
			'image_bytes': self.image_bytes,
			'total_bytes': self.total_bytes,
		}

def _stack_name(index: int) -> str:
	return f'stack_{index:06d}.mrcs'

def load(datadir: str | Path, spec: dict[str, Any]) -> Dataset:
	"""
	A dataset that has been generated under `datadir`.

	Raises:
		Missing: If it has not been generated there.
	"""
	path = Path(datadir) / name(spec)
	description = path / DESCRIPTION
	if not description.is_file():
		raise Missing(
			f"Dataset {name(spec)} is not under {datadir}. Make it with "
			f"`python -m vitrio_bench generate --datadir {datadir}` and the "
			"same --dataset options."
		)
	recorded = json.loads(description.read_text())
	return Dataset(path, recorded['spec'], recorded['sizes'])

def _noise(rng: np.random.Generator, shape: tuple, dtype: str) -> np.ndarray:
	if dtype == 'int16':
		return rng.integers(-32768, 32768, size=shape, dtype=np.int16)
	return rng.standard_normal(shape, dtype=np.float32).astype(dtype)

@contextlib.contextmanager
def _locked(path: Path) -> Iterator[None]:
	path.parent.mkdir(parents=True, exist_ok=True)
	with open(path, 'a', encoding='ascii') as file:
		if fcntl is not None:
			fcntl.flock(file, fcntl.LOCK_EX)
		yield

def generate(datadir: str | Path, spec: dict[str, Any]) -> Dataset:
	"""
	Generate a dataset under `datadir`, unless it is there already.

	It is written into a directory of its own, which takes its name only
	once every file is complete, so a generation that was interrupted is
	never taken for a dataset. Processes that generate the same dataset at
	once take turns, and all but the first find it done.
	"""
	with _locked(Path(datadir) / f'{name(spec)}.lock'):
		try:
			return load(datadir, spec)
		except Missing:
			return _write(datadir, spec)

def _write(datadir: str | Path, spec: dict[str, Any]) -> Dataset:
	import vitrio as vio

	final = Path(datadir) / name(spec)
	partial = final.with_name(final.name + '.partial')
	if partial.exists():
		shutil.rmtree(partial)
	partial.mkdir(parents=True)

	sizes = stack_sizes(spec)
	box = spec['box']
	dtype = spec['dtype']
	image_bytes = box * box * np.dtype(dtype).itemsize
	chunk = max(1, CHUNK_BYTES // image_bytes)

	writers = vio.writer_provider()
	saver = vio.saver(writers)
	for number, size in enumerate(sizes):
		path = partial / _stack_name(number)
		writers.declare(
			path,
			vio.ImageDescriptor((size, box, box), 2, vio.NumericalType(dtype))
		)
		rng = np.random.default_rng([spec['seed'], 0, number])
		for first in range(0, size, chunk):
			count = min(chunk, size - first)
			images = _noise(rng, (count, box, box), dtype)
			locations = [
				vio.ImageLocation(path, index)
				for index in range(first, first + count)
			]
			vio.write_batch_async(saver, images, locations).get()
		writers.close(path)

	(partial / DESCRIPTION).write_text(
		json.dumps(
			{'spec': spec, 'sizes': sizes, 'vitrio': vio.__version__},
			indent='\t', sort_keys=True
		)
	)
	if hasattr(os, 'sync'):
		os.sync()
	partial.rename(final)
	return Dataset(final, spec, sizes)
