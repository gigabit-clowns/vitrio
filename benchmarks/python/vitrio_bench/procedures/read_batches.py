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

from typing import Any

from ..datasets import Dataset
from ..readers import Reader
from . import _batches

WORKLOAD = _batches.WORKLOAD

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	return _batches.make_workload('read_batches', WORKLOAD, params, dataset)

def run(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Read the epochs of a workload, and measure them."""
	measured = _batches.measure(dataset, reader, workload, started)
	return _batches.metrics(measured, dataset, reader)
