# SPDX-License-Identifier: LGPL-2.1-or-later

"""The named sweeps of `zarr`: one question each.

A parameter given values here states its type, as the base test's do, so
that `-P` can still give it others from the command line."""

import os
import sys

import reframe as rfm
from reframe.core.builtins import parameter

sys.path.insert(0, os.path.dirname(__file__))

from vitrio_tests import Zarr    # noqa: E402

LIBRARIES = ['vitrio', 'zarr-python', 'tensorstore']

@rfm.simple_test
class zarr_libraries(Zarr):
	"""
	Where vitrio stands against zarr-python and tensorstore, writing and
	reading the same store: a volume and a tilt series, of floats and of
	integers.
	"""

	library = parameter(LIBRARIES, type=str)
	kind = parameter(['volume', 'stack'], type=str)
	dtype = parameter(['float32', 'int16'], type=str)

@rfm.simple_test
class zarr_cold_reads(Zarr):
	"""
	The same, its store's pages dropped before each read, so that the reads
	come from the storage.
	"""

	library = parameter(LIBRARIES, type=str)
	kind = parameter(['volume', 'stack'], type=str)
	store_cold = parameter(['fadvise'], type=str)

@rfm.simple_test
class zarr_workers(Zarr):
	"""
	What the threads of vitrio's pool are worth: a write and a read are a
	task per chunk, carried out at once on them.
	"""

	kind = parameter(['volume', 'stack'], type=str)
	workers = parameter([1, 2, 4, 8, 16, 32], type=int)

@rfm.simple_test
class zarr_volume_chunks(Zarr):
	"""
	The side of the cubes a volume is chunked in,
	`VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE`: smaller chunks cost more files
	and more calls, larger ones more than a patch needs.
	"""

	library = parameter(LIBRARIES, type=str)
	chunk_edge = parameter([32, 64, 128, 256], type=int)

@rfm.simple_test
class zarr_image_chunks(Zarr):
	"""
	The side of the squares the images of a stack are chunked in,
	`VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE`.
	"""

	library = parameter(LIBRARIES, type=str)
	kind = parameter(['stack'], type=str)
	chunk_edge = parameter([256, 512, 1024, 2048], type=int)
