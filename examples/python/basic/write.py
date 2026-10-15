#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Write image files.

`vio.write_single` writes an array as one image or volume, and
`vio.write_stack` as a stack of them along its first axis. The format is
the one the suffix of the path names: `.mrc` and `.mrcs` for MRC, `.tif`
and `.tiff` for TIFF. A numpy array, a tensor of PyTorch or JAX or a
`vitrio.Array` is written from where it is.

The example writes to a temporary directory and removes it afterwards, so
it needs no input and leaves nothing behind.

	write.py

Needs vitrio and numpy.
"""

import tempfile
from pathlib import Path

import numpy as np
import vitrio as vio

def main() -> None:
	rng = np.random.default_rng(0)
	image = rng.normal(size=(64, 96)).astype(np.float32)
	stack = rng.normal(size=(10, 64, 96)).astype(np.float32)

	with tempfile.TemporaryDirectory() as temporary:
		directory = Path(temporary)

		# One image, and a stack of ten. The same array is a volume when
		# it is written with `write_single`.
		vio.write_single(image, directory / 'image.mrc')
		vio.write_stack(stack, directory / 'stack.mrcs')
		vio.write_single(stack, directory / 'volume.mrc')
		for name in ('image.mrc', 'stack.mrcs', 'volume.mrc'):
			descriptor = vio.query_descriptor(directory / name)
			print(f"{name}: {descriptor.extents}, core rank "
				f"{descriptor.core_rank}")

		# The values are converted to the type the file stores.
		vio.write_single(
			image, directory / 'image_half.mrc', data_type='float16'
		)
		stored = vio.query_descriptor(directory / 'image_half.mrc')
		print(f"stored as {stored.data_type}")

		# The same array in another format, by naming another suffix.
		vio.write_stack(stack, directory / 'stack.tif')
		assert np.array_equal(
			np.asarray(vio.read(directory / 'stack.tif')), stack
		)

		# An `ImageDescriptor` states all of it at once: the extents, how
		# many of the last ones are an image, and the type stored. Here a
		# stack of ten becomes ten volumes of one image.
		descriptor = vio.ImageDescriptor(
			stack.shape, 2, vio.NumericalType('float32')
		)
		vio.write(stack, directory / 'described.mrcs', descriptor)

if __name__ == '__main__':
	main()
