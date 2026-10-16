# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `read_batches`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line."""

import os
import sys

import reframe as rfm
from reframe.core.builtins import parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import ReadBatches    # noqa: E402

@rfm.simple_test
class readers(ReadBatches):
	"""
	Every reader on both layouts: where vitrio stands against the ways
	other software reads.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	reader = parameter(
		[
			'vitrio-loader', 'vitrio-per-item', 'mrcfile-per-read',
			'mrcfile-open', 'numpy-memmap', 'preload',
		],
		type=str
	)
	cache = parameter([4096], type=int)

@rfm.simple_test
class workers(ReadBatches):
	"""
	H1 and H4: what the size of vitrio's pool is worth.

	A batch is read with one task per file, so a batch of one stack runs on
	one thread whatever the pool's size (H1), unless batches are read ahead.
	The size that suits the storage need not be the number of cores the
	loader takes by default, nor those the process may use (H4). The cache
	keeps every stack open, so that opening files stays out of it.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	workers = parameter([1, 2, 4, 8, 16, 32, 64], type=int)
	in_flight = parameter([0, 2, 8], type=int)
	cache = parameter([4096], type=int)

@rfm.simple_test
class cache(ReadBatches):
	"""
	H3: what the cache of open readers is worth, for each layout.

	`vio.loader` keeps none by default, and so opens a file for every
	batch that reads from it; the loader `vio.data` shares keeps 64.
	"""

	layout = parameter(
		['single-stack', 'per-micrograph:20', 'per-micrograph:100'], type=str
	)
	cache = parameter([0, 16, 64, 256, 4096], type=int)
