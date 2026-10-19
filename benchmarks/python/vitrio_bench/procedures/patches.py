# SPDX-License-Identifier: LGPL-2.1-or-later

"""Patches cropped out of the images of a dataset, as many per image, at
corners drawn at random: particles read out of micrographs without being
extracted first.

A batch of patches is read image by image: its rows are put in the order of
their images, as extraction reads them, so that a reader that crops the
patches of an image in one call is given them together.

Workload: that of `read_batches`, and
	patch: The side of a patch, in pixels.
	patches: How many patches each image has.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any

import numpy as np

from ..datasets import Dataset, draw_picks
from ..readers import Reader
from . import _batches

WORKLOAD = {**_batches.WORKLOAD, 'patch': 128, 'patches': 100}

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	patches = int(params.get('patches', WORKLOAD['patches']))
	workload = _batches.make_workload(
		'patches', WORKLOAD, params, dataset.count * patches
	)
	workload['patch'] = int(workload['patch'])
	workload['patches'] = patches
	return workload

def run(
	dataset: Dataset,
	reader: Reader,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""
	Crop the patches of the epochs of a workload, and measure them.

	Raises:
		ValueError: If the reader does not crop patches.
	"""
	if not reader.reads_patches:
		raise ValueError(f"{reader.name} does not crop patches.")
	picks = draw_picks(
		dataset, workload['patches'], workload['patch'], workload['seed']
	)

	def read(batches: list[np.ndarray]) -> Iterator[np.ndarray]:
		grouped = [
			rows[np.argsort(picks.image[rows], kind='stable')]
			for rows in batches
		]
		return reader.patches(grouped, picks)

	measured = _batches.measure(reader, workload, started, picks.image, read)
	elements = workload['patch'] * workload['patch']
	return _batches.metrics(
		measured, reader, elements * dataset.dtype.itemsize,
		elements * reader.dtype.itemsize
	)
