#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Keep the images of a training set in RAM from the first epoch on.

A program that goes through the same images, epoch after epoch, reads the
same files again each time. A scratch is a copy of them that is made once.
A loader built over one reads what the scratch holds from it, and takes in
what it does not hold yet. `prefetch_scratch_async` fills it ahead of the
reads, on threads of its own.

`run_length` is how many images of a file are taken in together, in one
read of the file.

When the images do not fit in RAM, or the next run should not read them
again, the scratch may live in a file mapped into memory instead
(`create_mapped_file_image_scratch`). A later scratch of the same images
over the same file resumes from what is already in it, except for the
images whose files were modified since, and programs that hold the same
images can share the file. vitrio never removes it, so remove it yourself
once nothing uses it.

To leave the files to vitrio, keep them in a directory with a budget
(`create_directory_image_scratch`). Each scratch gets a file of its own
there, found again by the next scratch of the same images, and the files
used least recently are removed to make room for new ones. Only the files
vitrio puts there count against the budget and are ever removed. The disk
needs room for the whole budget, since the files take their space as
images are taken in.

The example reads every image of a few stacks twice, through a scratch, and
checks both epochs against what the files hold.

	scratch_across_epochs.py

Needs vitrio and numpy.
"""

import tempfile
from pathlib import Path

import numpy as np
import vitrio as vio

STACKS = 3
IMAGES_PER_STACK = 32
SIZE = 24
BATCH_SIZE = 16

def main() -> None:
	rng = np.random.default_rng(0)
	with tempfile.TemporaryDirectory() as directory:
		stacks = []
		expected = {}
		for number in range(STACKS):
			path = Path(directory) / f'stack_{number}.mrcs'
			images = rng.normal(
				size=(IMAGES_PER_STACK, SIZE, SIZE)
			).astype(np.float32)
			vio.write_stack(images, path)
			stacks.append(path)
			for index, image in enumerate(images):
				expected[vio.ImageLocation(path, index)] = image

		locations = list(expected)

		# Holds all of them, in memory, and starts empty.
		scratch = vio.create_host_image_scratch(locations, run_length=8)

		# Or, when the images do not fit in RAM, keep the copy on fast
		# local storage, such as an NVMe disk, in a file mapped into
		# memory. It outlives the program, and the next run resumes from it:
		#
		# scratch = vio.create_mapped_file_image_scratch(
		# 	locations, "/local/disk/particles.scratch", run_length=8
		# )

		# The loader and the prefetch are separate pools, so that filling
		# the scratch does not hold up the reads that already need it.
		loader = vio.loader(workers=2, cache=len(stacks), scratch=scratch)
		prefetcher = vio.ThreadPoolExecutor(2)    # keep it while it works
		filling = vio.prefetch_scratch_async(scratch, locations, prefetcher)

		for epoch in range(2):
			for first in range(0, len(locations), BATCH_SIZE):
				batch = locations[first:first + BATCH_SIZE]
				images = np.empty((len(batch), SIZE, SIZE), np.float32)
				vio.read_batch_async(loader, images, batch).get()
				for location, image in zip(batch, images):
					assert np.array_equal(image, expected[location])
			print(f"epoch {epoch}: {len(locations)} images")

		filling.get()    # raises if the prefetch failed

if __name__ == '__main__':
	main()
