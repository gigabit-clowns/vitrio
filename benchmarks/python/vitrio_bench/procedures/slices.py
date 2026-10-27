# SPDX-License-Identifier: LGPL-2.1-or-later

"""Slices of a volume, along one of its axes: what a viewer of a tomogram
shows, and what a program that works a plane at a time reads.

The volume is the single stack of a dataset, which lies in its file as a
volume does: its images are the planes along `z`, the slowest axis. A slice
along `z` is one image, whose values follow one another. A slice along `y`
takes a row of every image, and one along `x` a value of every row, so they
read the whole volume in pieces of a row or of a value.

Workload: that of `read_batches`, whose rows are the slices, and
	axis: `z`, `y` or `x`.
"""

from __future__ import annotations

from typing import Any

import numpy as np

from ..datasets import Dataset
from ..readers import Reader
from . import _batches

WORKLOAD = {**_batches.WORKLOAD, 'axis': 'z'}
AXES = ('z', 'y', 'x')

def _require_volume(dataset: Dataset) -> None:
	if len(dataset.stacks) != 1:
		raise ValueError(
			"Slices are read out of a volume: a dataset of one stack, whose "
			"layout is 'single-stack'."
		)

def slice_count(dataset: Dataset, axis: str) -> int:
	"""How many slices a volume has along an axis."""
	return dataset.count if axis == 'z' else dataset.box

def slice_shape(dataset: Dataset, axis: str) -> tuple[int, int]:
	"""The shape of a slice along an axis."""
	if axis == 'z':
		return dataset.box, dataset.box
	return dataset.count, dataset.box

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot, or
			the dataset is not one volume.
	"""
	_require_volume(dataset)
	axis = params.get('axis', WORKLOAD['axis'])
	if axis not in AXES:
		raise ValueError(f"An axis is one of {AXES}, not {axis!r}.")
	workload = _batches.make_workload(
		'slices', WORKLOAD, params, slice_count(dataset, axis)
	)
	workload['axis'] = axis
	return workload

def run(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""
	Read the slices of the epochs of a workload, and measure them.

	Raises:
		ValueError: If the reader does not read slices.
	"""
	if not reader.reads_slices:
		raise ValueError(f"{reader.name} does not read slices.")
	axis = workload['axis']
	groups = np.zeros(slice_count(dataset, axis), dtype=np.int64)
	measured = _batches.measure(
		reader,
		workload,
		started,
		groups,
		lambda batches: reader.slices(batches, axis)
	)
	elements = int(np.prod(slice_shape(dataset, axis)))
	return _batches.metrics(
		measured, reader, elements * dataset.dtype.itemsize,
		elements * reader.dtype.itemsize
	)
