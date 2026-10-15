#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Read batches of images while the previous one is being used.

A read that is `_async` returns before its images are in memory, and hands
back a `Completion` to wait on. So the next batch can be on its way while
the current one is processed, which is how a program that goes through many
images keeps the disk and the processor busy at once.

Here you choose the pieces: the thread pool, the loader that runs on it,
how many open files it keeps, and the array every batch lands in. The
example reads shuffled images from several stacks, two batches at a time,
and checks that each batch holds the images it asked for.

	batches_in_flight.py

Needs vitrio and numpy.
"""

import random
import tempfile
from pathlib import Path

import numpy as np
import vitrio as vio

STACKS = 4
IMAGES_PER_STACK = 50
SIZE = 32
BATCH_SIZE = 16

def make_stacks(directory: Path) -> list[Path]:
	"""Write stacks whose image number `i` is filled with the value `i`."""
	paths = []
	for stack in range(STACKS):
		path = directory / f'stack_{stack}.mrcs'
		first = stack * IMAGES_PER_STACK
		values = np.arange(first, first + IMAGES_PER_STACK, dtype=np.float32)
		vio.write_stack(
			np.broadcast_to(
				values[:, None, None], (IMAGES_PER_STACK, SIZE, SIZE)
			),
			path
		)
		paths.append(path)
	return paths

def main() -> None:
	with tempfile.TemporaryDirectory() as temporary:
		stacks = make_stacks(Path(temporary))

		# Every image of every stack, in the order they will be read, and
		# the value each one was filled with.
		locations = [
			vio.ImageLocation(stack, index)
			for stack in stacks
			for index in range(vio.query_descriptor(stack).extents[0])
		]
		values = {
			location: number for number, location in enumerate(locations)
		}
		random.Random(0).shuffle(locations)
		size = vio.get_core_extents(vio.query_descriptor(stacks[0]))

		# Four threads read. The loader keeps as many stacks open as there
		# are, so that a batch does not open each one again.
		executor = vio.ThreadPoolExecutor(4)
		loader = vio.loader(cache=len(stacks), executor=executor)

		def start_reading(first):
			"""Start a batch, which lands in an array of its own."""
			batch = locations[first:first + BATCH_SIZE]
			images = np.empty((len(batch), *size), dtype=np.float32)
			return batch, images, vio.read_batch_async(loader, images, batch)

		total = 0
		current = start_reading(0)
		for first in range(BATCH_SIZE, len(locations), BATCH_SIZE):
			upcoming = start_reading(first)    # runs in the background
			total += check(*current, values)   # waits for the current one
			current = upcoming
		total += check(*current, values)
		print(f"{total} images read in batches of {BATCH_SIZE}")
		assert total == len(locations)

def check(batch, images, reading, values) -> int:
	"""Wait for a batch, and stand in for the work done on it."""
	reading.get()    # raises if any read failed
	for location, image in zip(batch, images):
		assert (image == values[location]).all()
	return len(batch)

if __name__ == '__main__':
	main()
