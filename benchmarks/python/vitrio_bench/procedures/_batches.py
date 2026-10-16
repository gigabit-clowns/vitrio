# SPDX-License-Identifier: LGPL-2.1-or-later

"""What the procedures that read batches share: their workload, the order of
an epoch, and going through epochs while measuring them."""

from __future__ import annotations

import hashlib
import time
from dataclasses import dataclass, field
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

def make_workload(
	procedure: str,
	defaults: dict[str, Any],
	params: dict[str, Any],
	dataset: Dataset
) -> dict[str, Any]:
	"""
	The complete workload of a procedure: the parameters given, and its
	defaults for the rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	unknown = set(params) - set(defaults)
	if unknown:
		raise ValueError(
			f"{procedure} has no workload parameter {sorted(unknown)}. It "
			f"has {sorted(defaults)}."
		)
	workload = {**defaults, **params}
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

@dataclass
class Measured:
	"""What going through the epochs measured."""

	open_s: float = 0.0
	wall_s: float = 0.0
	first_batch_s: float | None = None
	epoch_s: list[float] = field(default_factory=list)
	waits: list[list[float]] = field(default_factory=list)
	epoch_read_bytes: list[int | None] = field(default_factory=list)
	delivered: int = 0
	batch_hash: str | None = None
	tree: ProcessTree | None = None

def measure(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float,
	step_s: float = 0.0
) -> Measured:
	"""
	Open the reader, go through the epochs of a workload, and close it.

	After each batch the process sleeps `step_s`, standing in for a step of
	a model on a GPU: it releases the GIL, as such a step does, and takes a
	known time. `started` is when the process began, in the seconds of
	`time.time`, from which the time to the first batch is counted.
	"""
	batch = workload['batch']
	hasher = hashlib.blake2b(digest_size=16) if workload['verify'] else None
	measured = Measured(tree=ProcessTree())
	with measured.tree:
		begin = time.perf_counter()
		reader.open()
		measured.open_s = time.perf_counter() - begin
		try:
			read_before = measured.tree.read_bytes_now()
			for epoch in range(workload['epochs']):
				rows = order(dataset, workload, epoch)
				batches = [
					rows[first:first + batch]
					for first in range(0, len(rows), batch)
				]
				waits = []
				epoch_begin = time.perf_counter()
				iterator = reader.epoch(batches)
				while True:
					asked = time.perf_counter()
					try:
						images = next(iterator)
					except StopIteration:
						break
					waits.append(time.perf_counter() - asked)
					if measured.first_batch_s is None:
						measured.first_batch_s = time.time() - started
					if hasher is not None:
						hasher.update(np.ascontiguousarray(images).tobytes())
					measured.delivered += len(images)
					if step_s:
						time.sleep(step_s)
				measured.epoch_s.append(time.perf_counter() - epoch_begin)
				measured.waits.append(waits)
				read_after = measured.tree.read_bytes_now()
				measured.epoch_read_bytes.append(
					read_after - read_before
					if read_after is not None and read_before is not None
					else None
				)
				read_before = read_after
		finally:
			reader.close()
		measured.wall_s = time.perf_counter() - begin
	if hasher is not None:
		measured.batch_hash = hasher.hexdigest()
	return measured

def metrics(
	measured: Measured,
	dataset: Dataset,
	reader: Reader
) -> dict[str, Any]:
	"""The metrics every procedure that reads batches reports."""
	reading = sum(measured.epoch_s)
	delivered = measured.delivered
	delivered_image_bytes = dataset.box * dataset.box * reader.dtype.itemsize
	file_bytes = delivered * dataset.image_bytes
	every_wait = [wait for waits in measured.waits for wait in waits]
	result = {
		'wall_s': measured.wall_s,
		'open_s': measured.open_s,
		'first_batch_s': measured.first_batch_s,
		'epoch_s': measured.epoch_s,
		'images': delivered,
		'images_per_s': delivered / reading if reading else None,
		'payload_bytes_per_s': (
			delivered * delivered_image_bytes / reading if reading else None
		),
		'file_bytes_per_s': file_bytes / reading if reading else None,
		**percentiles(every_wait, 'wait'),
		**measured.tree.metrics(),
		'max_rss_bytes': max_rss_bytes(),
		**reader.metrics(),
	}
	cpu = result['cpu_s']
	result['cpu_per_image_s'] = cpu / delivered if delivered and cpu else None
	read = result['read_bytes']
	result['read_amplification'] = (
		read / file_bytes if read is not None and file_bytes else None
	)
	if measured.batch_hash is not None:
		result['batch_hash'] = measured.batch_hash
	return result
