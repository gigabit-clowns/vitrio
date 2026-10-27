# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `slices`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line.
"""

import os
import sys

import reframe as rfm
from reframe.core.builtins import parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import Slices    # noqa: E402

@rfm.simple_test
class slice_readers(Slices):
	"""
	Every reader that takes slices, along each axis of a tomogram: vitrio's
	against numpy's maps and mrcfile's. A slice along `z` is one plane of the
	file; one along `x` takes a value of every row of it.
	"""

	reader = parameter(
		['vitrio-loader', 'numpy-memmap-kept', 'mrcfile-mmap-kept'], type=str
	)
	axis = parameter(['z', 'y', 'x'], type=str)
