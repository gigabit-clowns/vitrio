# SPDX-License-Identifier: LGPL-2.1-or-later

"""Patches cropped out of the images of a dataset, as many per image, at
corners drawn at random: particles read out of micrographs without being
extracted first.

How the patches of a request are spread is the crowding:
- sparse: batches of patches drawn across the images in the order of the
  workload, a few of each image a batch, as a model trained on patches draws
  them;
- crowded: every patch of an image in one batch, an image after another, as
  extraction reads them, the patches covering 40% of an image unless told
  how many there are.

A batch of patches is read image by image: its rows are put in the order of
their images, so that a reader that crops the patches of an image in one
call is given them together.

Workload: that of `read_batches`, and
	patch: The side of a patch, in pixels.
	patches: How many patches each image has: 100 when sparse, and as many
		as cover 40% of an image when crowded.
	crowding: sparse or crowded. A crowded workload sets its order and its
		batches itself, and takes neither.
"""

from __future__ import annotations

from collections.abc import Iterator
from typing import Any

import numpy as np

from ..datasets import Dataset, draw_picks
from ..readers import Reader
from . import _batches

CROWDINGS = ('sparse', 'crowded')
SPARSE_PATCHES = 100
CROWDED_COVERAGE = 0.4

WORKLOAD = {
	**_batches.WORKLOAD, 'patch': 128, 'patches': None, 'crowding': 'sparse',
}

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The complete workload: the parameters given, and the defaults for the
	rest, written out.

	Raises:
		ValueError: If a parameter is unknown, or holds what it cannot.
	"""
	crowding = params.get('crowding', WORKLOAD['crowding'])
	if crowding not in CROWDINGS:
		raise ValueError(f"A crowding is one of {CROWDINGS}.")
	patch = int(params.get('patch', WORKLOAD['patch']))
	if not 0 < patch <= dataset.box:
		raise ValueError(f"Patches have sides of 1 to {dataset.box}.")
	patches = params.get('patches')
	if patches is None:
		patches = (
			SPARSE_PATCHES if crowding == 'sparse'
			else max(round(CROWDED_COVERAGE * dataset.box**2 / patch**2), 1)
		)
	patches = int(patches)
	if patches < 1:
		raise ValueError("Every image has at least one patch.")

	params = dict(params)
	if crowding == 'crowded':
		given = sorted({'order', 'batch'} & set(params))
		if given:
			raise ValueError(
				f"A crowded workload reads every patch of an image in a "
				f"batch, an image after another: it takes no {given}."
			)
		params.update(order='sequential', batch=patches)
	workload = _batches.make_workload(
		'patches', WORKLOAD, params, dataset.count * patches
	)
	workload.update(patch=patch, patches=patches, crowding=crowding)
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
