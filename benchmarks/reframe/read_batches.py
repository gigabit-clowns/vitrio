# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `read_batches`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line."""

import os
import sys

import reframe as rfm
from reframe.core.builtins import fixture, parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import ReadBatches, VitrioBuild    # noqa: E402

@rfm.simple_test
class readers(ReadBatches):
	"""
	Every reader on both layouts: where vitrio stands against the ways
	other software reads.
	"""

	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	reader = parameter(
		[
			'vitrio-loader', 'vitrio-per-item', 'mrcfile-mmap-per-read',
			'mrcfile-mmap-kept', 'mrcfile-open-last', 'numpy-memmap-kept',
			'python-read-kept', 'preload',
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

@rfm.simple_test
class dtype(ReadBatches):
	"""
	Reading fewer bytes: particles stored as float16 or int16 and delivered
	as float32, which vitrio converts as it reads, against the baselines,
	which convert after.
	"""

	dtype = parameter(['float32', 'float16', 'int16'], type=str)
	reader = parameter(
		['vitrio-loader', 'numpy-memmap-kept', 'mrcfile-mmap-kept'], type=str
	)
	cache = parameter([4096], type=int)

class BudgetBuild(VitrioBuild):
	"""vitrio built with each budget of a step of a read, 64 MiB by default."""

	budget = parameter(
		[1 << 20, 1 << 22, 1 << 24, 1 << 26, 1 << 28], type=int
	)

	def defines(self):
		return {'VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET': self.budget}

@rfm.simple_test
class prefetch_budget(ReadBatches):
	"""
	How many bytes a read should ask the kernel for at once, ahead of
	copying them, for one stack and for one stack per micrograph. Run it
	cold, with `-P cold=fadvise`, where asking ahead matters.
	"""

	vitrio = fixture(BudgetBuild, scope='environment')
	layout = parameter(['single-stack', 'per-micrograph:100'], type=str)
	cache = parameter([4096], type=int)
