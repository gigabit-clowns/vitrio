#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Convert an image file to another format, and maybe another data type.

The format of each file is the one its suffix names, so converting is
reading one and writing the other. What the file holds, an image, a stack
or a volume, is kept, and so is what it states that the other format can
carry, such as its pixel size.

	convert.py micrograph.mrc micrograph.tif
	convert.py stack.mrcs half.mrcs --data-type float16

Needs vitrio.
"""

from __future__ import annotations

import argparse
from collections.abc import Sequence
from pathlib import Path

import vitrio as vio

def parse_arguments(argv: Sequence[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Convert an image file to another format."
	)
	parser.add_argument('source', type=Path, help="file to read")
	parser.add_argument('destination', type=Path, help="file to write")
	parser.add_argument(
		'--data-type',
		help="data type to store the values as, such as float16 "
		"(default: that of the source)"
	)
	return parser.parse_args(argv)

def main(argv: Sequence[str] | None = None) -> None:
	arguments = parse_arguments(argv)

	# Reading and writing take the same descriptor of what the file holds,
	# so that is all it takes to keep it as it is.
	source = vio.query_descriptor(arguments.source)
	data_type = (
		vio.NumericalType(arguments.data_type)
		if arguments.data_type else source.data_type
	)
	destination = vio.ImageDescriptor(
		source.extents, source.core_rank, data_type
	)

	# What the source states beyond that goes with it. The format of the
	# destination writes what of it it can carry and leaves out the rest.
	vio.write(
		vio.read(arguments.source),
		arguments.destination,
		destination,
		metadata=vio.read_metadata(arguments.source)
	)

if __name__ == '__main__':
	main()
