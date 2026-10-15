#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Read image files.

`vio.read` works the format out from the file and returns an `Array` that
numpy, PyTorch and JAX use as it is, with no copy.

The example writes a stack to a temporary directory, so that it has
something to read, and removes it afterwards. It needs no input.

	read_stack.py

Needs vitrio and numpy.
"""

import tempfile
from pathlib import Path

import numpy as np
import vitrio as vio

def main() -> None:
	rng = np.random.default_rng(0)
	stack = rng.normal(size=(10, 64, 96)).astype(np.float32)

	with tempfile.TemporaryDirectory() as temporary:
		# Create the file to be read
		path = Path(temporary) / 'stack.mrcs'
		vio.write_stack(stack, path)

		# What a file holds can be asked for without reading it.
		descriptor = vio.query_descriptor(path)
		print(f"{path.name}: {descriptor.extents} of {descriptor.data_type}")

		# A whole file. `np.asarray` views the memory of the `Array`, and
		# nothing is copied.
		everything = np.asarray(vio.read(path))
		assert np.array_equal(everything, stack)

		# One image of a stack is named by an `ImageLocation`...
		third = np.asarray(vio.read(vio.ImageLocation(path, 2)))
		assert np.array_equal(third, stack[2])

		# ...which is also how RELION spells it, one-based, in a STAR file.
		# `ImageLocation.from_string` takes the spelling the package puts
		# in one.
		location = vio.ImageLocation.from_string(f"3@{path}")
		print(f"{location} is image {location.index_in_stack}")

		# The values are converted on the way in.
		half = np.asarray(vio.read(path, data_type=np.float16))
		print(f"read as {half.dtype}")

		# Memory of your own is read into where it is, which saves
		# allocating an array for each read.
		buffer = np.empty((64, 96), np.float32)
		vio.read(vio.ImageLocation(path, 5), out=buffer)
		assert np.array_equal(buffer, stack[5])

if __name__ == '__main__':
	main()
