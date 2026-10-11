# SPDX-License-Identifier: LGPL-2.1-or-later

"""The order rows are read in, a batch at a time."""

from __future__ import annotations

from collections.abc import Iterator

import numpy as np

class BatchSampler:
	"""
	Draws the rows of a dataset in batches, once each per epoch.

	Each iteration is an epoch. When it shuffles, every epoch has an order
	of its own, which the seed and the number of the epoch decide, so two
	samplers given one seed draw the same rows.

	A sampler can draw only one shard of the rows, for one of several
	processes that each read a part of every batch, as with PyTorch's
	DistributedSampler: the rows of an epoch are dealt out in turn, after
	the first ones are repeated until every shard has as many. The shards
	must be given one seed, so that they deal out the same order.
	"""

	def __init__(
		self,
		size: int,
		batch_size: int,
		shuffle: bool = False,
		drop_last: bool = False,
		seed: int | None = None,
		shard: tuple[int, int] = (0, 1)
	) -> None:
		"""
		Args:
			size: How many rows there are.
			batch_size: How many rows a batch has, the last one maybe fewer.
				With shards, the rows of a batch of one shard.
			shuffle: Whether to draw the rows in an order of each epoch's
				own. Otherwise they are drawn in their order.
			drop_last: Whether to leave out the last batch when it has
				fewer rows.
			seed: What decides the orders. Defaults to a seed of the
				sampler's own.
			shard: Which shard to draw, and of how many.

		Raises:
			ValueError: If the batch size is not positive, or the shard is
				not one of its count.
		"""
		if batch_size < 1:
			raise ValueError(f"A batch of {batch_size} rows holds none.")
		shard_index, shard_count = shard
		if shard_count < 1 or not 0 <= shard_index < shard_count:
			raise ValueError(
				f"There is no shard {shard_index} of {shard_count}."
			)
		if seed is None:
			seed = int(np.random.SeedSequence().entropy % 2**63)

		self.size = size
		self.batch_size = batch_size
		self.shuffle = shuffle
		self.drop_last = drop_last
		self.seed = seed
		self.shard = (shard_index, shard_count)
		self._epoch = 0

	@property
	def epoch(self) -> int:
		"""The epoch the next iteration draws."""
		return self._epoch

	def set_epoch(self, epoch: int) -> None:
		"""Choose the epoch the next iteration draws."""
		self._epoch = epoch

	def _rows_of_shard(self) -> int:
		shard_count = self.shard[1]
		return -(-self.size // shard_count)

	def __len__(self) -> int:
		"""How many batches an epoch has."""
		rows = self._rows_of_shard()
		if self.drop_last:
			return rows // self.batch_size
		return -(-rows // self.batch_size)

	def order(self, epoch: int) -> np.ndarray:
		"""The rows of the shard in the order an epoch draws them."""
		if self.shuffle:
			generator = np.random.default_rng([self.seed, epoch])
			rows = generator.permutation(self.size)
		else:
			rows = np.arange(self.size)

		shard_index, shard_count = self.shard
		if shard_count > 1:
			rows = np.resize(rows, self._rows_of_shard() * shard_count)
			rows = rows[shard_index::shard_count]
		return rows

	def __iter__(self) -> Iterator[np.ndarray]:
		epoch = self._epoch
		self._epoch += 1
		rows = self.order(epoch)
		for start in range(0, len(self) * self.batch_size, self.batch_size):
			yield rows[start:start + self.batch_size]
