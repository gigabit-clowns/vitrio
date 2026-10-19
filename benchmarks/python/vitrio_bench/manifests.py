# SPDX-License-Identifier: LGPL-2.1-or-later

"""Manifests of real datasets: where to download them, and how to tell that
what was downloaded is whole.

A manifest is a JSON file of `benchmarks/datasets/`. The root of a dataset is
the directory its `download` URL lists, and every path is relative to it.

- `files`: files whose size is known, each with `bytes`, and with `stack`
  (its `count`, `box` and `dtype`) when it is a stack of particles.
- `particles`: for a dataset of many stacks, the STAR file that names them,
  with how many particles and stacks it names, their `box` and `dtype`.
  Every stack it names is checked to be there, of that box and data type,
  and holding the particles asked of it.
- `notes` and `variants`: what else to know, and how to make the variants
  the benchmarks read, such as a smaller box or float16 on disk.

The data itself is never in this repository: whoever runs the benchmarks
downloads it.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .readers._mrc import layout

MANIFESTS = Path(__file__).resolve().parents[2] / 'datasets'

def names() -> list[str]:
	"""The names of the manifests there are."""
	return sorted(path.stem for path in MANIFESTS.glob('*.json'))

def load(name: str) -> dict[str, Any]:
	"""
	The manifest of a name, or at a path.

	Raises:
		FileNotFoundError: If there is none.
	"""
	path = Path(name)
	if not path.is_file():
		path = MANIFESTS / f'{name.lower()}.json'
	return json.loads(path.read_text())

def _check_stack(
	path: Path,
	stack: dict[str, Any],
	needed: int
) -> list[str]:
	shape, dtype, _ = layout(path)
	problems = []
	if shape[1:] != (stack['box'], stack['box']):
		problems.append(f"{path}: images of {shape[1:]}, not {stack['box']}².")
	if dtype.name != stack['dtype']:
		problems.append(f"{path}: {dtype.name}, not {stack['dtype']}.")
	if shape[0] < needed:
		problems.append(f"{path}: {shape[0]} images, {needed} asked of it.")
	return problems

def _check_particles(root: Path, particles: dict[str, Any]) -> list[str]:
	import starfile

	star = root / particles['star']
	if not star.is_file():
		return [f"{star} is missing."]
	blocks = starfile.read(star, always_dict=True)
	table = next(
		block for block in blocks.values()
		if hasattr(block, 'columns') and 'rlnImageName' in block.columns
	)
	parts = table['rlnImageName'].str.split('@', n=1)
	index = parts.str[0].astype(int)
	needed = index.groupby(parts.str[1], sort=False).max()

	problems = []
	if len(table) != particles['count']:
		problems.append(
			f"{star} names {len(table)} particles, not {particles['count']}."
		)
	if len(needed) != particles['stacks']:
		problems.append(
			f"{star} names {len(needed)} stacks, not {particles['stacks']}."
		)
	for name, count in needed.items():
		path = root / name
		if not path.is_file():
			problems.append(f"{path} is missing.")
			continue
		problems += _check_stack(path, particles, int(count))
	return problems

def check(manifest: dict[str, Any], root: str | Path) -> list[str]:
	"""
	Check that a dataset downloaded into `root` is what its manifest
	states.

	Returns:
		list[str]: What is wrong, nothing when it is whole.
	"""
	root = Path(root)
	problems = []
	for entry in manifest.get('files', []):
		path = root / entry['path']
		if not path.is_file():
			problems.append(f"{path} is missing.")
			continue
		size = path.stat().st_size
		if size != entry['bytes']:
			problems.append(f"{path}: {size} bytes, not {entry['bytes']}.")
			continue
		stack = entry.get('stack')
		if stack is not None:
			problems += _check_stack(path, stack, stack['count'])
	if 'particles' in manifest:
		problems += _check_particles(root, manifest['particles'])
	# A file both listed and named, such as the STAR file, is said once.
	return list(dict.fromkeys(problems))
