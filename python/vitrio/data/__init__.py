# SPDX-License-Identifier: LGPL-2.1-or-later

"""Feeding images and their metadata to a model, a batch at a time.

For instance::

	import vitrio as vio

	particles = starfile.read("particles.star", always_dict=True)["particles"]
	dataset = vio.data.Dataset({
		"image": vio.data.ImageSource(
			vio.data.parse_locations(particles["rlnImageName"]),
			data_type="float32"
		),
		"defocus": particles[["rlnDefocusU", "rlnDefocusV"]].to_numpy(),
	})
	for batch in vio.data.DataLoader(dataset, 64, shuffle=True):
		...

A `Dataset` is rows of named fields. A field is a source, which reads its
values from files (`ImageSource`, `PatchSource`) or holds them in memory
(`ArraySource`), or a column of metadata. A `DataLoader` goes through a
dataset in batches, reading the next ones on a thread pool meanwhile, and
hands them out as numpy arrays, tensors of PyTorch or arrays of JAX. The
metadata is parsed by whoever reads it: vitrio takes what it was parsed
into, and `as_columns`, `parse_locations` and `make_locations` turn that
into fields.

Needs numpy. PyTorch and JAX are imported only to hand batches to them.
This file is the whole public surface of the subpackage.
"""

from __future__ import annotations

try:
	import numpy as _numpy  # noqa: F401
except ImportError as error:  # pragma: no cover
	raise ImportError(
		"vitrio.data needs numpy, which is installed apart from vitrio."
	) from error

from ._dataset import (
	Dataset as Dataset,
	PendingBatch as PendingBatch,
)
from ._loader import DataLoader as DataLoader
from ._samplers import BatchSampler as BatchSampler
from ._sources import (
	ArraySource as ArraySource,
	ImageSource as ImageSource,
	PatchSource as PatchSource,
	Source as Source,
)
from ._tables import (
	as_columns as as_columns,
	make_locations as make_locations,
	parse_locations as parse_locations,
)

__all__ = [
	'ArraySource',
	'BatchSampler',
	'DataLoader',
	'Dataset',
	'ImageSource',
	'PatchSource',
	'PendingBatch',
	'Source',
	'as_columns',
	'make_locations',
	'parse_locations',
]
