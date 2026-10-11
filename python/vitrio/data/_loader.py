# SPDX-License-Identifier: LGPL-2.1-or-later

"""Going through a dataset in batches, with the next ones already on their
way."""

from __future__ import annotations

from collections import deque
from collections.abc import Callable, Iterable, Iterator, Sequence
from typing import Any

import numpy as np

from .._binding import ImageLoader
from . import _frameworks
from ._dataset import Allocator, Dataset, PendingBatch
from ._samplers import BatchSampler

OUTPUTS = ('numpy', 'torch', 'jax')

class DataLoader:
	"""
	Goes through a dataset in batches, reading the next ones meanwhile.

	Its arguments are those of PyTorch's DataLoader where they mean the
	same, but it starts no worker process: the images are read on the
	thread pool of its loader, `prefetch` batches at once, straight into
	the arrays they are handed out in. Each iteration is an epoch.

	The loader is what reads: its thread pool, its open readers and, when
	it was made with one, a scratch that serves the epochs after the first.
	Loaders that are given one loader share its thread pool.

	A batch is a dict with an array for each field of the dataset, the
	first axis of each running along the rows of the batch. It is handed
	out as numpy arrays, as tensors of PyTorch or as arrays of JAX. A field
	that does not hold numbers stays a numpy array.

	JAX compiles a function again for every shape it is given, so a last
	batch with fewer rows costs a compilation. `drop_last` leaves it out,
	and `pad_last` fills it up with rows of its own and says which rows are
	real in a field of booleans.

	Every batch is read into arrays of its own, which PyTorch and JAX may
	use without a copy, so a batch that has been handed out is never
	written again.
	"""

	def __init__(
		self,
		dataset: Dataset,
		batch_size: int = 1,
		shuffle: bool = False,
		*,
		batch_sampler: Iterable[Sequence[int]] | None = None,
		loader: ImageLoader | None = None,
		drop_last: bool = False,
		pad_last: bool = False,
		seed: int | None = None,
		prefetch: int = 4,
		output: str | Callable[[dict[str, np.ndarray]], Any] = 'numpy',
		device: Any = None,
		sharding: Any = None,
		pin_memory: bool | None = None,
		allocator: Allocator | None = None,
		transform: Callable[[Any], Any] | None = None,
		valid: str = 'valid'
	) -> None:
		"""
		Args:
			dataset: What to go through.
			batch_size: How many rows a batch has.
			shuffle: Whether to draw the rows in an order of each epoch's
				own.
			batch_sampler: What draws the rows of each batch, instead of a
				`BatchSampler` made of `batch_size`, `shuffle`, `drop_last`
				and `seed`. Any iterable of sequences of rows, such as a
				BatchSampler of PyTorch over its DistributedSampler.
			loader: Where reads of files are dispatched, such as
				`vio.loader(workers=8, scratch=scratch)`. Defaults to the
				loader shared by the process.
			drop_last: Whether to leave out the last batch when it has
				fewer rows.
			pad_last: Whether to fill the last batch up to `batch_size`
				rows by repeating its own, and give every batch a field of
				booleans, `valid`, false for the rows that repeat.
			seed: What decides the orders of the epochs. Defaults to a seed
				of the loader's own.
			prefetch: How many batches to have under way at once.
			output: What to hand the batches out as: `'numpy'`, `'torch'`,
				`'jax'`, or a callable given each batch as numpy arrays.
			device: Where to copy the batches to: a device of PyTorch for
				`'torch'`, or of JAX for `'jax'`. Defaults to leaving them
				in host memory for PyTorch, and to the default device of
				JAX.
			sharding: How JAX lays each field out across its devices, for
				`'jax'` only. A sharding over the devices of several
				processes makes each batch the share of this process.
			pin_memory: Whether to read into pinned memory, for `'torch'`
				only. Defaults to doing so when `device` is a GPU of CUDA.
			allocator: What makes the arrays of a batch, as
				`Dataset.read_async` takes it. Defaults to `numpy.empty`.
			transform: What to do with each batch once it is handed out
				as `output` says, such as normalizing it on the GPU.
			valid: The name of the field `pad_last` adds.

		Raises:
			ValueError: If a batch sampler is given with any of what it
				replaces, `drop_last` with `pad_last`, `output` is none of
				the above, or what is given does not apply to it.
		"""
		if batch_sampler is None:
			batch_sampler = BatchSampler(
				len(dataset), batch_size,
				shuffle=shuffle, drop_last=drop_last, seed=seed
			)
		elif shuffle or drop_last or seed is not None:
			raise ValueError(
				"shuffle, drop_last and seed are those of the batch "
				"sampler, so they cannot be given with one."
			)
		if drop_last and pad_last:
			raise ValueError("A last batch is either dropped or padded.")
		if pad_last and (valid in dataset.fields or valid == dataset.index):
			raise ValueError(
				f"The field {valid!r} would hide which rows are padding. "
				"Name it otherwise with `valid`."
			)
		if prefetch < 1:
			raise ValueError("At least the batch handed out is under way.")
		if not callable(output) and output not in OUTPUTS:
			raise ValueError(
				f"A batch is handed out as one of {OUTPUTS}, or to a "
				f"callable, not as {output!r}."
			)
		if sharding is not None and output != 'jax':
			raise ValueError("A sharding lays out arrays of JAX.")
		if sharding is not None and device is not None:
			raise ValueError("A sharding says on which devices it lies.")
		if pin_memory and output != 'torch':
			raise ValueError("Memory is pinned for PyTorch.")
		if pin_memory and allocator is not None:
			raise ValueError(
				"Pinned memory comes from an allocator of its own."
			)
		if device is not None and callable(output):
			raise ValueError("A callable output puts the batch where it wants.")

		self.dataset = dataset
		self.batch_sampler = batch_sampler
		self.loader = loader
		self.batch_size = getattr(batch_sampler, 'batch_size', batch_size)
		self.pad_last = pad_last
		self.prefetch = prefetch
		self.output = output
		self.device = device
		self.sharding = sharding
		self.pin_memory = pin_memory
		self.allocator = allocator
		self.transform = transform
		self.valid = valid

	def __len__(self) -> int:
		"""How many batches an epoch has."""
		return len(self.batch_sampler)

	def _pins_memory(self) -> bool:
		if self.pin_memory is not None:
			return self.pin_memory
		return self.output == 'torch' and _frameworks.is_cuda(self.device)

	def _start(
		self,
		rows: Sequence[int],
		allocator: Allocator | None
	) -> tuple[PendingBatch, np.ndarray | None]:
		rows = np.asarray(rows, dtype=np.intp)
		if not self.pad_last:
			return self.dataset.read_async(rows, allocator, self.loader), None

		valid = np.arange(max(self.batch_size, len(rows))) < len(rows)
		if 0 < len(rows) < self.batch_size:
			rows = np.resize(rows, self.batch_size)
		return self.dataset.read_async(rows, allocator, self.loader), valid

	def _hand_out(self, batch: dict[str, np.ndarray], pinned: bool) -> Any:
		if callable(self.output):
			result = self.output(batch)
		elif self.output == 'torch':
			# Only a copy out of pinned memory may overlap what follows.
			result = _frameworks.to_torch(
				batch, self.device, non_blocking=pinned
			)
		elif self.output == 'jax':
			result = _frameworks.to_jax(batch, self.device, self.sharding)
		else:
			result = batch

		if self.transform is not None:
			result = self.transform(result)
		return result

	def __iter__(self) -> Iterator[Any]:
		pinned = self._pins_memory()
		allocator = (
			_frameworks.pinned_allocator() if pinned else self.allocator
		)
		draws = iter(self.batch_sampler)
		window: deque[tuple[PendingBatch, np.ndarray | None]] = deque()
		for rows in draws:
			window.append(self._start(rows, allocator))
			if len(window) == self.prefetch:
				break

		# A batch is drawn before the one before it is waited for, so that
		# `prefetch` of them stay under way. Those left behind when the
		# iteration stops early are waited for once they are dropped.
		while window:
			pending, valid = window.popleft()
			following = next(draws, None)
			if following is not None:
				window.append(self._start(following, allocator))

			batch = pending.get()
			if valid is not None:
				batch[self.valid] = valid
			yield self._hand_out(batch, pinned)
