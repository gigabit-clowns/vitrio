# SPDX-License-Identifier: LGPL-2.1-or-later

"""A dataset written from memory, a batch of images at a time, in the order
of its stacks: what extraction, conversion and reconstruction write.

The dataset is not generated: its files are written into a directory of the
storage measured, which is removed afterwards. Its layout decides the files:
one stack, a stack per micrograph, or an image a file, as micrographs are.
Its data type is the one the files hold, so a dataset of `float16` written
from `float32` images converts every value.

A write is done once its files are on the storage. Writers that only leave
them in the page cache, as a mapping does, would otherwise be timed for less
than the others, so with `sync` the time runs until a sync of the whole
system returns.

Workload:
	batch: Images a batch hands the writer.
	sync: Whether the time runs until what was written is on the storage.
"""

from __future__ import annotations

import os
import shutil
import time
from pathlib import Path
from typing import Any

import numpy as np

from .. import writers
from ..datasets import Dataset
from ..metrics import ProcessTree, max_rss_bytes

WORKLOAD = {'batch': 256, 'sync': True}

# What a run of the procedure writes is a dataset of its own, not one
# generated beforehand.
writes = True

def make_reader(
	dataset: Dataset,
	params: dict[str, Any],
	scratchdir: str | Path | None
) -> writers.Writer:
	"""The writer a run names, `vitrio-saver` by default."""
	return writers.get(params.get('name', 'vitrio-saver'))(dataset, params)

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
			f"write_batches has no workload parameter {sorted(unknown)}. It "
			f"has {sorted(WORKLOAD)}."
		)
	workload = {**WORKLOAD, **params}
	workload['batch'] = int(workload['batch'])
	if workload['batch'] < 1:
		raise ValueError("A batch holds at least one image.")
	if not isinstance(workload['sync'], bool):
		raise ValueError("sync is true or false.")
	return workload

def run(
	dataset: Dataset,
	writer: writers.Writer,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Write the dataset, and measure it."""
	directory = dataset.path
	shutil.rmtree(directory, ignore_errors=True)
	directory.mkdir(parents=True)

	box = dataset.box
	batch = workload['batch']
	rng = np.random.default_rng(0)
	images = rng.standard_normal((batch, box, box), dtype=np.float32)
	images = images.astype(writer.dtype, copy=False)
	rows = np.arange(dataset.count)

	def batches():
		for first in range(0, dataset.count, batch):
			chunk = rows[first:first + batch]
			yield images[:len(chunk)], chunk

	tree = ProcessTree()
	try:
		with tree:
			begin = time.perf_counter()
			writer.open(directory)
			try:
				writer.write(batches())
			finally:
				writer.close()
			closed = time.perf_counter()
			if workload['sync'] and hasattr(os, 'sync'):
				os.sync()
			end = time.perf_counter()
	finally:
		shutil.rmtree(directory, ignore_errors=True)

	seconds = end - begin
	file_bytes = dataset.total_bytes
	result = {
		'images': dataset.count,
		'write_s': closed - begin,
		'sync_s': end - closed,
		'seconds': seconds,
		'images_per_s': dataset.count / seconds if seconds else None,
		'file_bytes_per_s': file_bytes / seconds if seconds else None,
		**tree.metrics(),
		'max_rss_bytes': max_rss_bytes(),
		**writer.metrics(),
	}
	cpu = result.get('cpu_s')
	result['cpu_per_image_s'] = cpu / dataset.count if cpu else None
	return result
