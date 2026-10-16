# SPDX-License-Identifier: LGPL-2.1-or-later

"""PyTorch's DataLoader over a dataset of one particle per item, read with
mrcfile: what most training code that reads particles looks like."""

from __future__ import annotations

from collections.abc import Iterable, Iterator, Sequence

import numpy as np

from . import Reader

class MrcfileParticles:
	"""
	A dataset of PyTorch: one particle per item, read by mapping its stack
	with mrcfile and copying the particle out, as cryoSTAR and DynaMight do.

	It is a plain class, which PyTorch takes as well as a subclass of its
	`Dataset`, so that worker processes can import it without importing
	PyTorch first.
	"""

	def __init__(
		self,
		paths: Sequence[str],
		stack_of: np.ndarray,
		index_of: np.ndarray,
		dtype: np.dtype
	) -> None:
		self.paths = list(paths)
		self.stack_of = stack_of
		self.index_of = index_of
		self.dtype = dtype

	def __len__(self) -> int:
		return len(self.stack_of)

	def __getitem__(self, row: int) -> np.ndarray:
		import mrcfile

		path = self.paths[self.stack_of[row]]
		with mrcfile.mmap(path, mode='r', permissive=True) as mrc:
			data = mrc.data
			image = data[self.index_of[row]] if data.ndim == 3 else data
			return np.array(image, dtype=self.dtype)

class _Batches:
	"""
	The batches of the current epoch, which a DataLoader that keeps its
	workers between epochs goes through anew each time.
	"""

	def __init__(self) -> None:
		self.batches: list[np.ndarray] = []

	def __iter__(self):
		return iter(self.batches)

	def __len__(self) -> int:
		return len(self.batches)

class TorchDataloader(Reader):
	"""
	Goes through the batches with `torch.utils.data.DataLoader`, which
	collates the particles of each batch into a tensor.

	Parameters:
		num_workers: Worker processes. 0 reads in the main process, which
			is PyTorch's default.
		persistent_workers: Whether the workers outlive an epoch.
		prefetch_factor: Batches each worker reads ahead. Defaults to
			PyTorch's, 2 when there are workers.
	"""

	name = 'torch-dataloader'
	requires = ('torch', 'mrcfile')
	defaults = {
		'num_workers': 0,
		'persistent_workers': False,
		'prefetch_factor': None,
	}

	def open(self) -> None:
		import torch.utils.data

		self._batches = _Batches()
		dataset = MrcfileParticles(
			[str(path) for path in self.dataset.stacks],
			self.stack_of, self.index_of, self.dtype
		)
		workers = self.params['num_workers']
		options = {}
		if workers > 0:
			options['persistent_workers'] = self.params['persistent_workers']
			if self.params['prefetch_factor'] is not None:
				options['prefetch_factor'] = self.params['prefetch_factor']
		self._loader = torch.utils.data.DataLoader(
			dataset, batch_sampler=self._batches, num_workers=workers,
			**options
		)

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		self._batches.batches = [np.asarray(rows).tolist() for rows in batches]
		for images in self._loader:
			yield images.numpy()

	def close(self) -> None:
		# Workers that persist end with their DataLoader.
		self._loader = None
