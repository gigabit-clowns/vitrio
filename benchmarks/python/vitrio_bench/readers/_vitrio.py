# SPDX-License-Identifier: LGPL-2.1-or-later

"""What vitrio's readers share: the loader, its open readers and its scratch.

The scratch is made anew for every run, so that each one measures it
filling up and being read. Resuming one from an earlier run is a question
of its own.
"""

from __future__ import annotations

import shutil
from pathlib import Path
from typing import Any

from . import Reader, default_workers

SCRATCHES = ('none', 'host', 'file', 'directory')

# The parameters of a loader, and their defaults, which are those of
# `vio.loader`: as many threads as the machine reports, no open readers
# kept, no scratch.
LOADER = {
	'workers': None,
	'cache': 0,
	'scratch': 'none',
	'run_length': 64,
	'budget': None,
}

# The room a directory scratch is given beyond what the images take, for
# the flags and the header it keeps besides them.
BUDGET_SLACK = 1.25

def scratch_path(reader: Reader) -> Path:
	"""Where a scratch of a reader is kept, for those kept in files."""
	name = (
		f"vitrio-bench-{reader.dataset.name}-{reader.params['scratch']}-"
		f"{reader.params['run_length']}"
	)
	return reader.require_scratchdir() / name

def make_loader(reader: Reader) -> tuple[Any, list[Any]]:
	"""
	The loader a reader's parameters ask for, and the location of each
	particle of its dataset.

	Raises:
		ValueError: If the scratch is none of `SCRATCHES`, or is kept in
			files and the run was given no place for them.
	"""
	import vitrio as vio

	params = reader.params
	if params['workers'] is None:
		params['workers'] = default_workers()
	kind = params['scratch']
	if kind not in SCRATCHES:
		raise ValueError(f"A scratch is one of {SCRATCHES}, not {kind!r}.")

	stacks = [str(path) for path in reader.dataset.stacks]
	locations = [
		vio.ImageLocation(stacks[stack], int(index))
		for stack, index in zip(reader.stack_of, reader.index_of)
	]

	scratch = None
	run_length = params['run_length']
	if kind == 'host':
		scratch = vio.create_host_image_scratch(locations, run_length)
	elif kind == 'file':
		path = scratch_path(reader)
		path.unlink(missing_ok=True)
		# Builds before the storage of a scratch went through transfers
		# keep one in a file through a mapping, which is how they compare.
		create = getattr(
			vio, 'create_file_image_scratch', None
		) or vio.create_mapped_file_image_scratch
		scratch = create(locations, path, run_length)
	elif kind == 'directory':
		directory = scratch_path(reader)
		shutil.rmtree(directory, ignore_errors=True)
		directory.mkdir(parents=True)
		if params['budget'] is None:
			params['budget'] = int(
				reader.dataset.total_bytes * BUDGET_SLACK
			)
		scratch = vio.create_directory_image_scratch(
			locations, directory, run_length, params['budget']
		)

	loader = vio.loader(
		workers=params['workers'],
		cache=params['cache'] or None,
		scratch=scratch
	)
	return loader, locations

def _disk_bytes(path: Path) -> int:
	# Windows counts no blocks, and keeps no file sparse unless asked to,
	# so there a file takes its size.
	status = path.stat()
	blocks = getattr(status, 'st_blocks', None)
	return status.st_size if blocks is None else blocks * 512

def scratch_bytes(reader: Reader) -> int | None:
	"""The bytes the files of a reader's scratch take on disk."""
	if reader.params['scratch'] not in ('file', 'directory'):
		return None
	path = scratch_path(reader)
	paths = path.rglob('*') if path.is_dir() else [path]
	return sum(_disk_bytes(item) for item in paths if item.is_file())

def remove_scratch(reader: Reader) -> None:
	"""Remove the files of a reader's scratch, once nothing holds it."""
	if reader.params['scratch'] in ('file', 'directory'):
		path = scratch_path(reader)
		if path.is_dir():
			shutil.rmtree(path, ignore_errors=True)
		else:
			path.unlink(missing_ok=True)
