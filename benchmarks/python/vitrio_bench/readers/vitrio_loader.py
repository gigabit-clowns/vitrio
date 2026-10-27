# SPDX-License-Identifier: LGPL-2.1-or-later

"""vitrio as it is meant to be used: a loader on a thread pool, each batch
read in one call, and the next batches under way while one is used."""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable, Iterator

import numpy as np

from ..datasets import Picks
from . import Reader
from ._vitrio import LOADER, make_loader, remove_scratch, scratch_bytes

class VitrioLoader(Reader):
	"""
	Reads each batch with one `read_batch_async` call.

	Parameters:
		workers: Threads of the loader's pool. Defaults to what vitrio's
			loader runs when given none.
		cache: Open readers the loader keeps. 0 keeps none, which is the
			default of `vio.loader`.
		scratch: Where the loader keeps copies of the particles it has
			read: `none`, `host` memory, a `file`, or a file of a
			`directory` kept within a budget. The last two are kept in the
			run's scratch directory.
		run_length: Particles of a file the scratch takes in together.
		budget: Bytes a directory scratch may take. Defaults to what the
			dataset takes, and a margin.
		in_flight: Batches read ahead of the one being used. 0 reads each
			batch only when it is asked for.
	"""

	name = 'vitrio-loader'
	defaults = {**LOADER, 'in_flight': 2}
	reads_patches = True
	reads_slices = True

	def open(self) -> None:
		import vitrio as vio

		self._vio = vio
		self._loader, self._locations = make_loader(self)
		self._scratch_bytes = None

	def _start(self, rows: np.ndarray):
		images = self.new_batch(len(rows))
		locations = [self._locations[row] for row in rows]
		return images, self._vio.read_batch_async(
			self._loader, images, locations
		)

	def epoch(self, batches: Iterable[np.ndarray]) -> Iterator[np.ndarray]:
		pending = deque()
		for rows in batches:
			pending.append(self._start(rows))
			if len(pending) > self.params['in_flight']:
				images, reading = pending.popleft()
				reading.get()
				yield images
		while pending:
			images, reading = pending.popleft()
			reading.get()
			yield images

	def _start_patches(self, rows: np.ndarray, picks: Picks):
		images = self.new_patches(len(rows), picks.patch)
		image_of = picks.image[rows]
		firsts = np.flatnonzero(np.r_[True, image_of[1:] != image_of[:-1]])
		lasts = np.r_[firsts[1:], len(rows)]
		readings = [
			self._vio.read_patches_async(
				self._loader,
				images[first:last],
				self._locations[image_of[first]],
				picks.corner[rows[first:last]] + picks.patch // 2
			)
			for first, last in zip(firsts, lasts)
		]
		return images, readings

	def patches(
		self,
		batches: Iterable[np.ndarray],
		picks: Picks
	) -> Iterator[np.ndarray]:
		"""
		Crops the patches of each image of a batch with one
		`read_patches_async` call, and reads batches ahead as `epoch` does.
		"""
		pending = deque()
		for rows in batches:
			pending.append(self._start_patches(rows, picks))
			if len(pending) > self.params['in_flight']:
				images, readings = pending.popleft()
				for reading in readings:
					reading.get()
				yield images
		while pending:
			images, readings = pending.popleft()
			for reading in readings:
				reading.get()
			yield images

	def _start_slices(self, rows: np.ndarray, axis: str):
		# A slice is a patch of the volume one value deep along its axis,
		# whose centre lands at the middle of the others.
		count, box = self.dataset.count, self.dataset.box
		if axis == 'z':
			shape = (1, box, box)
			centres = [(int(row), box // 2, box // 2) for row in rows]
		elif axis == 'y':
			shape = (count, 1, box)
			centres = [(count // 2, int(row), box // 2) for row in rows]
		else:
			shape = (count, box, 1)
			centres = [(count // 2, box // 2, int(row)) for row in rows]
		slices = self.new_slices(len(rows), axis)
		reading = self._vio.read_patches_async(
			self._loader,
			slices.reshape((len(rows), *shape)),
			self._vio.ImageLocation(str(self.dataset.stacks[0])),
			centres
		)
		return slices, reading

	def slices(
		self,
		batches: Iterable[np.ndarray],
		axis: str
	) -> Iterator[np.ndarray]:
		"""
		Reads the slices of a batch with one `read_patches_async` call, and
		reads batches ahead as `epoch` does.
		"""
		pending = deque()
		for rows in batches:
			pending.append(self._start_slices(rows, axis))
			if len(pending) > self.params['in_flight']:
				slices, reading = pending.popleft()
				reading.get()
				yield slices
		while pending:
			slices, reading = pending.popleft()
			reading.get()
			yield slices

	def close(self) -> None:
		# Destroying the loader joins its threads and releases its scratch.
		self._loader = None
		self._locations = None
		self._scratch_bytes = scratch_bytes(self)
		remove_scratch(self)

	def metrics(self) -> dict:
		return {'scratch_bytes': self._scratch_bytes}
