# SPDX-License-Identifier: LGPL-2.1-or-later

"""The readers a procedure can be given: vitrio's, and the baselines.

Every reader reads batches of particles of a dataset, in the order it is
asked for, into arrays of the data type it delivers. What sets them apart
is how: vitrio in batches with reads under way, or the ways other software
reads, one particle at a time.

A reader names the modules it needs in `requires` and its parameters, with
their defaults, in `defaults`. A reader whose modules are missing is not
run, and its run is recorded as skipped.
"""

from __future__ import annotations

import importlib.util
import os
from collections.abc import Iterable, Iterator
from typing import Any, ClassVar

import numpy as np

from ..datasets import Dataset

class Skip(Exception):
	"""A reader cannot run here, for the reason it gives."""

class Reader:
	"""
	Reads batches of particles of a dataset.

	A subclass opens what it reads with in `open`, hands out one array per
	batch in `epoch`, and releases what it opened in `close`.
	"""

	name: ClassVar[str]
	requires: ClassVar[tuple[str, ...]] = ()
	defaults: ClassVar[dict[str, Any]] = {}

	def __init__(self, dataset: Dataset, params: dict[str, Any]) -> None:
		allowed = {'name', 'dtype', *self.defaults}
		unknown = set(params) - allowed
		if unknown:
			raise ValueError(
				f"The reader {self.name} has no parameter {sorted(unknown)}. "
				f"It has {sorted(allowed - {'name'})}."
			)
		self.dataset = dataset
		self.params = {
			'dtype': 'float32',
			**self.defaults,
			**{key: value for key, value in params.items() if key != 'name'},
		}
		self.dtype = np.dtype(self.params['dtype'])
		self.stack_of, self.index_of = dataset.locate()

	def new_batch(self, size: int) -> np.ndarray:
		"""An array for a batch of `size` particles, as it is delivered."""
		box = self.dataset.box
		return np.empty((size, box, box), dtype=self.dtype)

	def open(self) -> None:
		"""Open what the reader reads with."""

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		"""Hand out the particles of each batch of rows, in order."""
		raise NotImplementedError

	def close(self) -> None:
		"""Release what `open` opened."""

	def describe(self) -> dict[str, Any]:
		"""
		The reader and every one of its settings, as a record states
		them.
		"""
		return {'name': self.name, **self.params}

def missing(reader: type[Reader]) -> list[str]:
	"""The modules a reader needs that cannot be imported here."""
	return [
		module for module in reader.requires
		if importlib.util.find_spec(module) is None
	]

def default_workers() -> int:
	"""The number of threads vitrio's loader runs when given none."""
	return os.cpu_count() or 1

from .mrcfile_open import MrcfileOpen    # noqa: E402
from .mrcfile_per_read import MrcfilePerRead    # noqa: E402
from .numpy_memmap import NumpyMemmap    # noqa: E402
from .preload import Preload    # noqa: E402
from .vitrio_loader import VitrioLoader    # noqa: E402
from .vitrio_per_item import VitrioPerItem    # noqa: E402

READERS: dict[str, type[Reader]] = {
	reader.name: reader
	for reader in (
		VitrioLoader, VitrioPerItem, MrcfilePerRead, MrcfileOpen,
		NumpyMemmap, Preload,
	)
}

def get(name: str) -> type[Reader]:
	"""
	The reader of a name.

	Raises:
		ValueError: If no reader has that name.
	"""
	try:
		return READERS[name]
	except KeyError:
		raise ValueError(
			f"There is no reader {name!r}. There are {sorted(READERS)}."
		) from None
