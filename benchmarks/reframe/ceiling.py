# SPDX-License-Identifier: LGPL-2.1-or-later

"""The storage's ceiling, which every reader is held against."""

import os
import sys

import reframe as rfm

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import Ceiling    # noqa: E402

@rfm.simple_test
class ceiling(Ceiling):
	"""
	fio on the largest file of the dataset: sequential reads, and random
	reads of a particle's size at queue depths 1, 4, 16 and 64.
	"""
