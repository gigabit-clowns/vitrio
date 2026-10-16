# SPDX-License-Identifier: LGPL-2.1-or-later

"""Batches of particles read as fast as possible, with nothing done with them.

With no compute to hide behind, what this measures is the reader alone: how
fast it delivers, how much CPU it spends doing so, and how many bytes it
takes from storage for the bytes it delivers.

Workload:
	order: `'sequential'`, the particles as the files hold them;
		`'shuffled'`, every particle in an order of the epoch's own; or
		`'shuffled-within'`, the files in order and the particles of each
		shuffled.
	batch: Particles per batch.
	images: How many particles an epoch reads, the first ones of its
		order. Defaults to all of them.
	epochs: How many times the particles are read.
	seed: What decides the orders of the epochs.
	verify: Whether to hash every batch, so that readers can be checked
		against each other. It costs CPU time between batches, so it is off
		when measuring.
"""

from __future__ import annotations

import hashlib
import time
from typing import Any

import numpy as np

from ..datasets import Dataset
from ..metrics import ProcessTree, max_rss_bytes, percentiles
from ..readers import Reader

WORKLOAD = {
	'order': 'shuffled',
	'batch': 256,
	'images': None,
	'epochs': 1,
	'seed': 0,
	'verify': False,
}
ORDERS = ('sequential', 'shuffled', 'shuffled-within')

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	unknown = set(params) - set(WORKLOAD)
	if unknown:
		raise ValueError(
			f"read_batches has no workload parameter {sorted(unknown)}. It "
			f"has {sorted(WORKLOAD)}."
		)
	workload = {**WORKLOAD, **params}
	if workload['order'] not in ORDERS:
		raise ValueError(f"An order is one of {ORDERS}.")
	if workload['images'] is None:
		workload['images'] = dataset.count
	workload['images'] = min(int(workload['images']), dataset.count)
	workload['batch'] = int(workload['batch'])
	workload['epochs'] = int(workload['epochs'])
	if workload['batch'] < 1 or workload['epochs'] < 1:
		raise ValueError("An epoch has batches of at least one particle.")
	return workload

def order(
	dataset: Dataset,
	workload: dict[str, Any],
	epoch: int
) -> np.ndarray:
	"""The particles an epoch reads, in the order it reads them."""
	rng = np.random.default_rng([workload['seed'], epoch])
	if workload['order'] == 'sequential':
		rows = np.arange(dataset.count)
	elif workload['order'] == 'shuffled':
		rows = rng.permutation(dataset.count)
	else:
		stack, _ = dataset.locate()
		rows = np.lexsort((rng.random(dataset.count), stack))
	return rows[:workload['images']]

def run(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Read the epochs of a workload, and measure them."""
	batch = workload['batch']
	hasher = hashlib.blake2b(digest_size=16) if workload['verify'] else None
	epochs = []
	waits = []
	first_batch = None
	delivered = 0

	tree = ProcessTree()
	with tree:
		begin = time.perf_counter()
		reader.open()
		opened = time.perf_counter()
		try:
			for epoch in range(workload['epochs']):
				rows = order(dataset, workload, epoch)
				batches = [
					rows[first:first + batch]
					for first in range(0, len(rows), batch)
				]
				epoch_begin = time.perf_counter()
				iterator = reader.epoch(batches)
				while True:
					asked = time.perf_counter()
					try:
						images = next(iterator)
					except StopIteration:
						break
					waits.append(time.perf_counter() - asked)
					if first_batch is None:
						first_batch = time.time() - started
					if hasher is not None:
						hasher.update(np.ascontiguousarray(images).tobytes())
					delivered += len(images)
				epochs.append(time.perf_counter() - epoch_begin)
		finally:
			reader.close()
		wall = time.perf_counter() - begin

	reading = sum(epochs)
	delivered_image_bytes = dataset.box * dataset.box * reader.dtype.itemsize
	file_bytes = delivered * dataset.image_bytes
	metrics = {
		'wall_s': wall,
		'open_s': opened - begin,
		'first_batch_s': first_batch,
		'epoch_s': epochs,
		'images': delivered,
		'images_per_s': delivered / reading if reading else None,
		'payload_bytes_per_s': (
			delivered * delivered_image_bytes / reading if reading else None
		),
		'file_bytes_per_s': file_bytes / reading if reading else None,
		**percentiles(waits, 'wait'),
		**tree.metrics(),
		'max_rss_bytes': max_rss_bytes(),
	}
	metrics['cpu_per_image_s'] = (
		metrics['cpu_s'] / delivered if delivered and metrics['cpu_s'] else None
	)
	metrics['read_amplification'] = (
		metrics['read_bytes'] / file_bytes
		if metrics['read_bytes'] is not None and file_bytes else None
	)
	if hasher is not None:
		metrics['batch_hash'] = hasher.hexdigest()
	return metrics
