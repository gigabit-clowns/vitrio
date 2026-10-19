# SPDX-License-Identifier: LGPL-2.1-or-later

"""Several epochs through a reader, with a step of known length after each
batch standing in for a model.

The step sleeps, which releases the GIL as a step on a GPU does, so that no
GPU is needed to tell how much of an epoch was spent waiting for data: the
stalled fraction. The first epoch is reported apart from the others, since
that is where a scratch fills up and a converted copy is made.

Workload: that of `read_batches`, with three epochs by default, and
	step_s: How long the step after each batch takes, in seconds. 0 reads
		as fast as the reader can.
"""

from __future__ import annotations

import statistics
from typing import Any

from ..datasets import Dataset
from ..readers import Reader
from . import _batches

WORKLOAD = {**_batches.WORKLOAD, 'epochs': 3, 'step_s': 0.0}

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	workload = _batches.make_workload(
		'epochs', WORKLOAD, params, dataset.count
	)
	workload['step_s'] = float(workload['step_s'])
	if workload['step_s'] < 0:
		raise ValueError("A step takes no less than no time.")
	return workload

def _later(values: list) -> float | None:
	later = [value for value in values[1:] if value is not None]
	return statistics.median(later) if later else None

def run(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Go through the epochs of a workload, and measure them."""
	stacks, _ = dataset.locate()
	measured = _batches.measure(
		reader, workload, started, stacks, step_s=workload['step_s']
	)
	result = _batches.metrics(
		measured, reader, dataset.image_bytes,
		dataset.box * dataset.box * reader.dtype.itemsize
	)

	epochs = measured.epoch_s
	stalls = [sum(waits) for waits in measured.waits]
	per_epoch = workload['images']
	batches = -(-per_epoch // workload['batch'])
	result.update({
		'first_epoch_s': epochs[0],
		'later_epoch_s': _later(epochs),
		'first_epoch_images_per_s': per_epoch / epochs[0],
		'later_images_per_s': (
			per_epoch / _later(epochs) if len(epochs) > 1 else None
		),
		'stall_s': stalls,
		'stall_fraction': sum(stalls) / sum(epochs),
		'first_stall_fraction': stalls[0] / epochs[0],
		'later_stall_fraction': _later([
			stall / epoch for stall, epoch in zip(stalls, epochs)
		]),
		'ideal_epoch_s': batches * workload['step_s'],
		'epoch_read_bytes': measured.epoch_read_bytes,
		'first_epoch_read_bytes': measured.epoch_read_bytes[0],
		'later_epoch_read_bytes': _later(measured.epoch_read_bytes),
	})
	return result
