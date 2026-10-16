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
from pathlib import Path
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

	def __init__(
		self,
		dataset: Dataset,
		params: dict[str, Any],
		scratchdir: str | Path | None = None
	) -> None:
		"""
		Args:
			dataset: What to read.
			params: The reader's parameters, as `defaults` names them.
			scratchdir: Where a reader may keep copies of the dataset,
				such as a scratch or a converted database. That is fast local
				storage as a rule, and a reader that needs it refuses to run
				without it.
		"""
		allowed = {'name', 'dtype', *self.defaults}
		unknown = set(params) - allowed
		if unknown:
			raise ValueError(
				f"The reader {self.name} has no parameter {sorted(unknown)}. "
				f"It has {sorted(allowed - {'name'})}."
			)
		self.dataset = dataset
		self.scratchdir = Path(scratchdir) if scratchdir is not None else None
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

	def unavailable(self) -> str | None:
		"""Why the reader cannot run here, or None if it can."""
		needed = missing(type(self))
		return f"{self.name} needs {needed}." if needed else None

	def files(self) -> list[Path]:
		"""
		The copies of the dataset the reader keeps between runs, such as a
		converted database, which a cold run drops from the cache as it does
		the dataset. Only those that exist before `open` count.
		"""
		return []

	def metrics(self) -> dict[str, Any]:
		"""
		What the reader measured of itself, such as the time it took to
		convert the dataset, to add to the metrics of the run.
		"""
		return {}

	def require_scratchdir(self) -> Path:
		"""
		The directory the reader keeps its copies in, made if it is
		missing.

		Raises:
			ValueError: If the run was given none.
		"""
		if self.scratchdir is None:
			raise ValueError(
				f"{self.name} keeps a copy of the dataset, and needs a place "
				"for it: --environment.scratchdir."
			)
		self.scratchdir.mkdir(parents=True, exist_ok=True)
		return self.scratchdir

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

from .lmdb_store import LmdbStore    # noqa: E402
from .mrcfile_open import MrcfileOpen    # noqa: E402
from .mrcfile_per_read import MrcfilePerRead    # noqa: E402
from .numpy_memmap import NumpyMemmap    # noqa: E402
from .preload import Preload    # noqa: E402
from .torch_dataloader import TorchDataloader    # noqa: E402
from .vitrio_data import VitrioData    # noqa: E402
from .vitrio_loader import VitrioLoader    # noqa: E402
from .vitrio_per_item import VitrioPerItem    # noqa: E402

READERS: dict[str, type[Reader]] = {
	reader.name: reader
	for reader in (
		VitrioLoader, VitrioPerItem, VitrioData, MrcfilePerRead, MrcfileOpen,
		NumpyMemmap, TorchDataloader, Preload, LmdbStore,
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
