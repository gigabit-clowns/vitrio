# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `write_batches`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line.
"""

import os
import sys

import reframe as rfm
from reframe.core.builtins import parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import WriteBatches    # noqa: E402

@rfm.simple_test
class writers(WriteBatches):
	"""
	Every writer on the layouts Cryo-EM software writes: a stack of
	particles, a stack per micrograph, and micrographs a file each.
	"""

	writer = parameter(['vitrio-saver', 'mrcfile-mmap'], type=str)
	layout = parameter(
		['single-stack', 'per-micrograph:100', 'one-per-file'], type=str
	)

	def dataset_options(self):
		# Micrographs are written at their size, 32 of them.
		if self.layout == 'one-per-file':
			return [
				'--dataset.count=32', '--dataset.layout=one-per-file',
				'--dataset.box=4096', f'--dataset.dtype={self.dtype}',
				f'--dataset.seed={self.seed}',
			]
		return super().dataset_options()

@rfm.simple_test
class write_dtype(WriteBatches):
	"""
	Particles stored as float16 or int16 from float32 images, which every
	value is converted for, against float32 stored as it is.
	"""

	dtype = parameter(['float32', 'float16', 'int16'], type=str)
	writer = parameter(['vitrio-saver', 'mrcfile-mmap'], type=str)
