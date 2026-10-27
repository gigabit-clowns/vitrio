# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `patches`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line.
"""

import os
import sys

import reframe as rfm
from reframe.core.builtins import fixture, parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import Patches, VitrioBuild    # noqa: E402

@rfm.simple_test
class patch_readers(Patches):
	"""
	Every reader that crops patches, at three sides: vitrio's, a call per
	micrograph of a batch, against crops one patch at a time through mrcfile,
	mapped and not, numpy's maps and plain reads of Python's files.
	"""

	reader = parameter(
		[
			'vitrio-loader', 'mrcfile-mmap-per-read', 'mrcfile-mmap-kept',
			'mrcfile-open-last', 'numpy-memmap-kept', 'python-read-kept',
		],
		type=str
	)
	patch = parameter([64, 128, 256], type=int)
	cache = parameter([4096], type=int)

class GapCapBuild(VitrioBuild):
	"""vitrio built with each widest gap a read bridges, 128 KiB by default."""

	gap_cap = parameter([1 << 12, 1 << 15, 1 << 17, 1 << 20], type=int)

	def defines(self):
		return {'VITRIO_DEFAULT_PREFETCH_GAP_CAP': self.gap_cap}

@rfm.simple_test
class gap_cap(Patches):
	"""
	H7: the rows of a patch lie a micrograph's row apart, and a read bridges
	the gaps between them up to a cap. Bridging more reads bytes no patch
	needs; bridging less asks for more stretches. Run it cold, with
	`-P cold=fadvise`, for the bytes read to mean anything.
	"""

	vitrio = fixture(GapCapBuild, scope='environment')
	patch = parameter([64, 256], type=int)
	cache = parameter([4096], type=int)
