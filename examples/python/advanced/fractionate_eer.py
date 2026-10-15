#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Fractionate the movies of an electron-counting detector.

An EER file holds the events a detector counted, thousands of frames of
them. This program cuts the time of each into a number of fractions, counts
the events of each fraction per pixel and stores the fractions as a movie
in an MRC stack or a TIFF file, which is what motion correction takes.

	fractionate_eer.py movies/*.eer -o fractions -n 40 --binning 2

A fraction gathers as many frames as it takes, as evenly as the frames
allow: no frame is dropped, and a fraction has at most one more than
another. `--binning` gathers pixels of the detector along each axis, which
has to divide its size. The fractions of a movie are read in parallel,
each on a thread of its own, since each is decoded from its own frames.

The movies are written to `--output-dir`, named after their sources.

Needs vitrio and numpy, built with TIFF, which EER files are read through.
"""

from __future__ import annotations

import argparse
import os
from collections.abc import Sequence
from pathlib import Path

import numpy as np
import vitrio as vio

SUFFIXES = {'mrc': '.mrcs', 'tiff': '.tif'}
# What both formats store of the integers and floats that counts make.
DATA_TYPES = ('uint8', 'uint16', 'float16', 'float32')

def positive_int(text: str) -> int:
	value = int(text)
	if value < 1:
		raise argparse.ArgumentTypeError(f"{value} is not positive")
	return value

def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Fractionate EER movies into MRC or TIFF stacks."
	)
	parser.add_argument('movies', nargs='+', type=Path, help="EER files")
	parser.add_argument(
		'-o', '--output-dir', type=Path, required=True,
		help="directory to write the fractionated movies to"
	)
	parser.add_argument(
		'-n', '--fractions', type=positive_int, required=True,
		help="how many fractions to cut each movie into"
	)
	parser.add_argument(
		'--binning', type=positive_int, default=1,
		help="pixels of the detector gathered into one along each axis "
		"(default: %(default)s)"
	)
	parser.add_argument(
		'--format', choices=sorted(SUFFIXES), default='mrc',
		help="format of the output (default: %(default)s)"
	)
	parser.add_argument(
		'--data-type', choices=DATA_TYPES, default='float32',
		help="data type of the output (default: %(default)s)"
	)
	parser.add_argument(
		'-j', '--threads', type=positive_int, default=os.cpu_count() or 1,
		help="threads decoding (default: %(default)s)"
	)
	return parser.parse_args(argv)

def fractionate(
	movie: Path,
	loader: vio.ImageLoader,
	readers: vio.ImageReaderProvider,
	destination: Path,
	data_type: str
) -> None:
	"""Read the fractions of one movie in parallel, and store them."""
	# The provider shows a movie as a stack with an image per fraction.
	descriptor = vio.query_descriptor(movie, readers)
	fractions = descriptor.extents[0]
	counts = np.empty(descriptor.extents, np.dtype(data_type))
	locations = [vio.ImageLocation(movie, i) for i in range(fractions)]
	vio.read_batch_async(loader, counts, locations).get()
	vio.write_stack(counts, destination)

def main(argv: Sequence[str] | None = None) -> None:
	arguments = parse_arguments(argv)
	arguments.output_dir.mkdir(parents=True, exist_ok=True)

	readers = vio.detector_event_image_reader_provider(
		arguments.fractions, binning=arguments.binning
	)
	loader = vio.ExecutorImageLoader(
		readers, vio.ThreadPoolExecutor(arguments.threads)
	)

	for movie in arguments.movies:
		destination = (
			arguments.output_dir / movie.with_suffix(
				SUFFIXES[arguments.format]
			).name
		)
		fractionate(
			movie, loader, readers, destination, arguments.data_type
		)
		print(f"{movie} -> {destination}")

if __name__ == '__main__':
	main()
