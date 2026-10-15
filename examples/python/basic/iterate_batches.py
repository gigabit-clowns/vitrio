#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Go through a dataset of images in shuffled batches.

`vio.data` is the highest level of vitrio. A `Dataset` is rows of named
fields: images read from files and whatever metadata goes with them. A
`DataLoader` goes through it in batches, reading the next ones on a thread
pool while the current one is used.

The example makes a few stacks of small images, each filled with its own
number, and a label for each image. It then goes through them for two
epochs, checking that every image arrives with the label that goes with it.
It replaces what a training loop does with the batch.

	iterate_batches.py

Needs vitrio and numpy. Ask for `output="torch"` or `output="jax"` to get
tensors instead, which also needs PyTorch or JAX.
"""

import tempfile
from pathlib import Path

import numpy as np
import vitrio as vio

STACKS = 3
IMAGES_PER_STACK = 20
SIZE = 16
BATCH_SIZE = 8

def make_stacks(directory: Path) -> list[vio.ImageLocation]:
	"""Write the stacks. Image `i` is filled with the value `i`."""
	locations = []
	for stack in range(STACKS):
		values = stack * IMAGES_PER_STACK + np.arange(IMAGES_PER_STACK)
		images = np.broadcast_to(
			values.astype(np.float32)[:, None, None],
			(IMAGES_PER_STACK, SIZE, SIZE)
		)
		path = directory / f'stack_{stack}.mrcs'
		vio.write_stack(images, path)
		locations += [
			vio.ImageLocation(path, index)
			for index in range(IMAGES_PER_STACK)
		]
	return locations

def main() -> None:
	with tempfile.TemporaryDirectory() as temporary:
		locations = make_stacks(Path(temporary))
		labels = np.arange(len(locations)) % 2

		# The images are not read here, only described. The metadata is
		# whatever arrays you already have: a column of a DataFrame, the
		# fields of a structured array.
		dataset = vio.data.Dataset({
			"image": vio.data.ImageSource(locations, data_type=np.float32),
			"label": labels,
		})
		print(f"{len(dataset)} rows")

		loader = vio.data.DataLoader(
			dataset, batch_size=BATCH_SIZE, shuffle=True, seed=0
		)
		for epoch in range(2):
			seen = 0
			for batch in loader:
				images, rows = batch["image"], batch["index"]
				# The row each image came from is in the batch, so
				# whatever else is known of it can be looked up.
				assert images.shape == (len(rows), SIZE, SIZE)
				assert np.array_equal(images[:, 0, 0], rows)
				assert np.array_equal(batch["label"], labels[rows])
				seen += len(rows)
			print(f"epoch {epoch}: {seen} images")
			assert seen == len(dataset)

if __name__ == '__main__':
	main()
