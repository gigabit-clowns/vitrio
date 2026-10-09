#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Extract particles from micrographs into MRC stacks.

Reads the STAR files RELION writes: one with the micrographs, and one that
pairs each micrograph with the file of the coordinates picked on it. A box
is cropped around each coordinate and stored in a stack, and a STAR file
describing the particles is written, which RELION reads.

	extract_particles.py -m micrographs.star -c coordinate_files.star \\
		-o particles.star -b 256

The particles go to a single stack named after the output, `particles.mrcs`
here, unless `--stack-file` names another or `--stack-dir` asks for a stack
per micrograph.

A coordinate is truncated to a whole pixel and lands at `box_size // 2`
within its box, which is where RELION puts it. A box that reaches past the
edge of its micrograph is padded with zeros. Nothing else is done to the
particles: they are neither normalized nor inverted.

Paths are used as the STAR files spell them, so a relative one is relative
to the working directory.

Needs vitrio, numpy, starfile and tqdm.
"""

from __future__ import annotations

import argparse
import os
from collections import Counter, deque
from collections.abc import Iterable, Iterator, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import TypeVar

import numpy as np
import pandas as pd
import starfile
import vitrio as vio
from tqdm import tqdm

MICROGRAPH_NAME = 'rlnMicrographName'
COORDINATE_FILE = 'rlnMicrographCoordinates'
# Slowest axis first, which is the order vitrio indexes an image in.
CENTRE = ['rlnCoordinateY', 'rlnCoordinateX']
IMAGE_NAME = 'rlnImageName'
MICROGRAPH_PIXEL_SIZE = 'rlnMicrographPixelSize'
IMAGE_PIXEL_SIZE = 'rlnImagePixelSize'
IMAGE_SIZE = 'rlnImageSize'
IMAGE_DIMENSIONALITY = 'rlnImageDimensionality'

STACK_SUFFIX = '.mrcs'
# The real valued types an MRC file holds.
DATA_TYPES = ('int8', 'uint8', 'int16', 'uint16', 'float16', 'float32')

# How many particles of a micrograph are read in one call, and how many such
# calls are under way at once. Their product bounds the memory the particles
# in flight take.
BATCH_SIZE = 128
BATCHES_IN_FLIGHT = 8

T = TypeVar('T')

@dataclass(frozen=True, eq=False)
class Batch:
	"""Particles of one micrograph, read in one call and written in one."""

	micrograph: vio.ImageLocation
	centres: np.ndarray
	destinations: list[vio.ImageLocation]

def positive_int(text: str) -> int:
	value = int(text)
	if value < 1:
		raise argparse.ArgumentTypeError(f"{value} is not positive")
	return value

def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Extract particles from micrographs into MRC stacks."
	)
	parser.add_argument(
		'-m', '--micrographs', type=Path, required=True,
		help="STAR file with the micrographs, as RELION writes it"
	)
	parser.add_argument(
		'-c', '--coordinates', type=Path, required=True,
		help="STAR file pairing each micrograph with its coordinate file"
	)
	parser.add_argument(
		'-o', '--output', type=Path, required=True,
		help="STAR file to write the particles to"
	)
	parser.add_argument(
		'-b', '--box-size', type=positive_int, required=True,
		help="side of the box cropped around each coordinate, in pixels"
	)
	stacks = parser.add_mutually_exclusive_group()
	stacks.add_argument(
		'--stack-file', type=Path,
		help="write every particle to this stack (default: the output "
		f"with the suffix {STACK_SUFFIX})"
	)
	stacks.add_argument(
		'--stack-dir', type=Path,
		help="write a stack for each micrograph under this directory "
		"instead"
	)
	parser.add_argument(
		'--data-type', choices=DATA_TYPES,
		help="data type of the stacks (default: that of the micrographs)"
	)
	parser.add_argument(
		'-j', '--threads', type=positive_int, default=os.cpu_count() or 1,
		help="threads reading and writing (default: %(default)s)"
	)
	return parser.parse_args(argv)

def read_particles(
	coordinates: Path,
	micrographs: pd.DataFrame
) -> pd.DataFrame:
	"""List the particles, each with what is known of its micrograph."""
	files = starfile.read(coordinates)
	unknown = set(files[MICROGRAPH_NAME]) - set(micrographs[MICROGRAPH_NAME])
	if unknown:
		raise SystemExit(
			f"{len(unknown)} micrographs of {coordinates} are not among the "
			f"micrographs, such as {min(unknown)}"
		)

	picked = [
		starfile.read(file).assign(**{MICROGRAPH_NAME: micrograph})
		for micrograph, file in zip(
			files[MICROGRAPH_NAME], files[COORDINATE_FILE]
		)
	]
	particles = pd.concat(picked, ignore_index=True)
	if particles.empty:
		raise SystemExit(f"{coordinates} holds no coordinates to extract")

	inherited = [
		column for column in micrographs.columns
		if column == MICROGRAPH_NAME or column not in particles.columns
	]
	return particles.merge(
		micrographs[inherited], on=MICROGRAPH_NAME, validate='many_to_one'
	)

def locate_micrograph(path: str) -> vio.ImageLocation:
	"""Address the one image a micrograph file holds."""
	extents = vio.query_descriptor(path).extents
	if len(extents) == 2:
		return vio.ImageLocation(path)
	if len(extents) == 3 and extents[0] == 1:
		return vio.ImageLocation(path, 0)
	raise SystemExit(f"{path} is not a single image: it is {extents}")

def name_stacks(
	micrographs: pd.Series,
	stack_file: Path,
	stack_dir: Path | None
) -> pd.Series:
	"""Name the stack each particle goes to."""
	if stack_dir is None:
		return pd.Series(str(stack_file), index=micrographs.index)
	return micrographs.map(
		lambda name: str(stack_dir / Path(name).with_suffix(STACK_SUFFIX).name)
	)

def assign_destinations(stacks: pd.Series) -> list[vio.ImageLocation]:
	"""Give each particle the next free image of its stack."""
	indices = stacks.groupby(stacks).cumcount()
	return [vio.ImageLocation(*place) for place in zip(stacks, indices)]

class OutputStacks:
	"""The stacks being written, each closed some time after it is complete.

	Closing a stack flushes it, which holds up whoever asks for it. So a
	stack whose last particle is stored is only queued, and closed when
	`close_one` is called.
	"""

	def __init__(
		self,
		writers: vio.FileImageWriterProvider,
		destinations: Iterable[vio.ImageLocation],
		box_size: int,
		data_type: vio.NumericalType
	) -> None:
		"""Declare every stack at its final size."""
		self._writers = writers
		self._awaited = Counter(place.key for place in destinations)
		self._complete: deque[str] = deque()
		for path, size in self._awaited.items():
			Path(path).parent.mkdir(parents=True, exist_ok=True)
			writers.declare(
				path,
				vio.ImageDescriptor(
					(size, box_size, box_size), 2, data_type
				)
			)

	def mark_stored(
		self,
		destinations: Iterable[vio.ImageLocation]
	) -> None:
		"""Queue the stacks these particles were the last ones of."""
		arrived = Counter(place.key for place in destinations)
		self._awaited.subtract(arrived)
		self._complete.extend(
			path for path in arrived if not self._awaited[path]
		)

	def close_one(self) -> bool:
		"""Close a complete stack, and say whether there was one."""
		if not self._complete:
			return False
		self._writers.close(self._complete.popleft())
		return True

def make_batches(
	particles: pd.DataFrame,
	destinations: Sequence[vio.ImageLocation]
) -> Iterator[Batch]:
	"""Split the particles of each micrograph into batches."""
	centres = particles[CENTRE].to_numpy().astype(np.int64)
	by_micrograph = particles.groupby(MICROGRAPH_NAME, sort=False).indices
	for name, rows in by_micrograph.items():
		micrograph = locate_micrograph(name)
		for start in range(0, len(rows), BATCH_SIZE):
			chunk = rows[start:start + BATCH_SIZE]
			yield Batch(
				micrograph,
				centres[chunk],
				[destinations[row] for row in chunk]
			)

def keep_in_flight(items: Iterable[T]) -> Iterator[T]:
	"""Yield the items in order, with some of them already drawn.

	Drawing an item is what starts its work. So when one is handed over to
	be waited for, the next `BATCHES_IN_FLIGHT` are under way.
	"""
	window: deque[T] = deque()
	for item in items:
		window.append(item)
		if len(window) == BATCHES_IN_FLIGHT:
			yield window.popleft()
	yield from window

def wait(completion: vio.Completion, stacks: OutputStacks) -> None:
	"""Wait for a completion, closing complete stacks in the meantime."""
	while not completion.is_ready and stacks.close_one():
		pass
	completion.get()

def start_reads(
	batches: Iterable[Batch],
	loader: vio.ImageLoader,
	box_size: int,
	dtype: np.dtype
) -> Iterator[tuple[Batch, np.ndarray, vio.Completion]]:
	"""Start cropping the particles of each batch into an array of its own."""
	for batch in batches:
		# Zeroed, since a box that reaches past the micrograph is read only
		# as far as the micrograph goes.
		shape = (len(batch.destinations), box_size, box_size)
		patches = np.zeros(shape, dtype)
		reading = vio.read_patches_async(
			loader, patches, batch.micrograph, batch.centres
		)
		yield batch, patches, reading

def start_writes(
	reads: Iterable[tuple[Batch, np.ndarray, vio.Completion]],
	saver: vio.ImageSaver,
	stacks: OutputStacks
) -> Iterator[tuple[Batch, vio.Completion]]:
	"""Start storing each batch once it has been read."""
	for batch, patches, reading in reads:
		wait(reading, stacks)
		yield batch, vio.write_batch_async(
			saver, patches, batch.destinations
		)

def extract(
	particles: pd.DataFrame,
	destinations: Sequence[vio.ImageLocation],
	box_size: int,
	dtype: np.dtype,
	threads: int
) -> None:
	"""Crop every particle out of its micrograph and store it."""
	executor = vio.ThreadPoolExecutor(threads)
	loader = vio.loader(cache=BATCHES_IN_FLIGHT, executor=executor)
	writers = vio.writer_provider()
	saver = vio.saver(writers, executor=executor)
	stacks = OutputStacks(
		writers, destinations, box_size, vio.NumericalType(dtype)
	)

	batches = make_batches(particles, destinations)
	reads = start_reads(batches, loader, box_size, dtype)
	writes = start_writes(keep_in_flight(reads), saver, stacks)
	with tqdm(total=len(particles), unit='particle') as progress:
		for batch, writing in keep_in_flight(writes):
			wait(writing, stacks)
			stacks.mark_stored(batch.destinations)
			progress.update(len(batch.destinations))

	while stacks.close_one():
		pass

def describe_images(
	optics: pd.DataFrame,
	box_size: int
) -> pd.DataFrame:
	"""Add to the optics groups what RELION asks of extracted particles."""
	return optics.assign(**{
		IMAGE_PIXEL_SIZE: optics[MICROGRAPH_PIXEL_SIZE],
		IMAGE_SIZE: box_size,
		IMAGE_DIMENSIONALITY: 2,
	})

def main(argv: Sequence[str] | None = None) -> None:
	arguments = parse_arguments(argv)

	blocks = starfile.read(arguments.micrographs, always_dict=True)
	particles = read_particles(arguments.coordinates, blocks['micrographs'])

	stacks = name_stacks(
		particles[MICROGRAPH_NAME],
		arguments.stack_file or arguments.output.with_suffix(STACK_SUFFIX),
		arguments.stack_dir
	)
	destinations = assign_destinations(stacks)

	first_micrograph = particles[MICROGRAPH_NAME].iloc[0]
	data_type = (
		arguments.data_type
		or vio.query_descriptor(first_micrograph).data_type.name
	)

	extract(
		particles, destinations,
		arguments.box_size, np.dtype(data_type), arguments.threads
	)

	particles.insert(0, IMAGE_NAME, [str(place) for place in destinations])
	output = {'particles': particles}
	if 'optics' in blocks:
		optics = describe_images(blocks['optics'], arguments.box_size)
		output = {'optics': optics, **output}
	starfile.write(output, arguments.output)

if __name__ == '__main__':
	main()
