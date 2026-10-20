# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `epochs`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line.
"""

import os
import sys

import reframe as rfm
import reframe.utility.typecheck as typ
from reframe.core.builtins import parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import Epochs    # noqa: E402

@rfm.simple_test
class scratch(Epochs):
	"""
	H2 and H5: what each kind of scratch is worth, and what it costs.

	A scratch is opened through the file it holds the copies of, so without
	a cache of open readers every batch still opens its files, even when
	the scratch holds everything (H2). A scratch takes in the runs of one
	file one after another, so filling it from a single stack is serial,
	and the first epoch is slower than the storage allows (H5).
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	scratch = parameter(['none', 'host', 'mapped', 'directory'], type=str)
	cache = parameter([0, 4096], type=int)

@rfm.simple_test
class run_length(Epochs):
	"""How many particles of a file a scratch should take in together."""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	scratch = parameter(['mapped'], type=str)
	run_length = parameter([1, 8, 64, 256], type=int)
	cache = parameter([4096], type=int)

@rfm.simple_test
class larger_than_ram(Epochs):
	"""
	H6: a dataset from a quarter of the memory to four times it.

	The dataset takes 2.6 GB, and the memory of the run, page cache
	included, is limited. Past the limit every epoch reads from storage.
	A mapped scratch on the same storage holds the same bytes, and is read
	with the same advice vitrio gives the pages of a stack, so past the
	limit it should read no more than the stack does.
	"""

	count = 40000
	cold = parameter(['fadvise'], type=str)
	memory_max = parameter(['10G', '5G', '2600M', '1300M', '650M'], type=str)
	scratch = parameter(['none', 'mapped'], type=str)
	cache = parameter([4096], type=int)

@rfm.simple_test
class overlap(Epochs):
	"""
	How many batches to read ahead, against the length of a step.

	Once reading a batch takes less than a step, a single batch read ahead
	should leave nothing to wait for.
	"""

	step_s = parameter([0.002, 0.008, 0.032], type=float)
	in_flight = parameter([0, 1, 2, 4, 8], type=int)
	cache = parameter([4096], type=int)

@rfm.simple_test
class loaders(Epochs):
	"""
	vitrio against the loaders training code uses: PyTorch's DataLoader
	reading with mrcfile, everything preloaded, and a conversion to LMDB
	first. Each is set up at its best: every stack kept open, eight
	workers that outlive an epoch.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	reader = parameter(
		['vitrio-loader', 'vitrio-data', 'torch-dataloader', 'preload', 'lmdb'],
		type=str
	)
	cache = parameter([4096], type=int)
	num_workers = parameter([8], type=int)
	persistent_workers = True

@rfm.simple_test
class torch_workers(Epochs):
	"""
	How many workers PyTorch's DataLoader needs, so that it is compared at
	its best.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	reader = parameter(['torch-dataloader'], type=str)
	num_workers = parameter([0, 1, 2, 4, 8, 16], type=int)
	persistent_workers = True

@rfm.simple_test
class convert_first(Epochs):
	"""
	The first run of a converted dataset, which converts it, against a
	rerun, which finds the conversion done, with and without the kernel's
	readahead. A rerun that finds no conversion converts all the same, and
	its `converted` metric says so.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	reader = parameter(['lmdb'], type=str)
	reuse = parameter([False, True], type=typ.Bool)
	readahead = parameter([False, True], type=typ.Bool)
