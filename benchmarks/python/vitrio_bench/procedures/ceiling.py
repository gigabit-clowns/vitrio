# SPDX-License-Identifier: LGPL-2.1-or-later

"""What the storage behind a dataset can deliver, measured with fio.

Every reader is held against this ceiling: once one reaches it, the only
ways to be faster are to read fewer bytes or to read them fewer times. fio
reads the largest file of the dataset with direct I/O, past the page cache:
once sequentially in large blocks, then at random in blocks the size of a
particle, at several queue depths. Then it writes a file of its own beside
the dataset sequentially, in the same blocks, synced at the end and removed
after, which is the ceiling of the writers.

The "reader" of this procedure is fio, with these parameters:
	fio: The fio to run.
	ioengine: fio's engine. One that keeps requests in flight, such as
		`libaio` or `io_uring`, for queue depths above 1.
	direct: Whether to read past the page cache.
	runtime_s: Seconds each measurement lasts.
	depths: The queue depths of the random reads.
	sequential_block: Bytes per sequential read, and write.
	sequential_depth: Queue depth of the sequential reads, and writes.
	write_bytes: The size of the file written, over and over while the
		measurement lasts; 0 writes nothing.

It has no workload.
"""

from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path
from typing import Any

from ..datasets import Dataset

FIO = {
	'fio': 'fio',
	'ioengine': 'libaio',
	'direct': True,
	'runtime_s': 10,
	'depths': [1, 4, 16, 64],
	'sequential_block': 1 << 20,
	'sequential_depth': 16,
	'write_bytes': 1 << 30,
}
WORKLOAD: dict[str, Any] = {}

# Direct I/O reads whole blocks of the device, so a block is rounded up to
# this.
ALIGNMENT = 4096

class Fio:
	"""fio, with the settings it measures the ceiling with."""

	name = 'fio'

	def __init__(self, params: dict[str, Any]) -> None:
		allowed = {'name', *FIO}
		unknown = set(params) - allowed
		if unknown:
			raise ValueError(
				f"fio has no parameter {sorted(unknown)}. It has {sorted(FIO)}."
			)
		given = {key: value for key, value in params.items() if key != 'name'}
		self.params = {**FIO, **given}

	def unavailable(self) -> str | None:
		"""Why fio cannot run here, or None if it can."""
		if shutil.which(self.params['fio']) is None:
			return f"{self.params['fio']} is not installed."
		return None

	def files(self) -> list[Path]:
		"""fio keeps no copy of the dataset."""
		return []

	def describe(self) -> dict[str, Any]:
		"""fio and its settings, as a record states them."""
		version = None
		if self.unavailable() is None:
			version = subprocess.run(
				[self.params['fio'], '--version'],
				capture_output=True, text=True, check=False
			).stdout.strip()
		return {'name': self.name, 'version': version, **self.params}

def make_reader(
	dataset: Dataset,
	params: dict[str, Any],
	scratchdir: str | Path | None
) -> Fio:
	"""fio, with the parameters a run gives it."""
	return Fio(params)

def make_workload(params: dict[str, Any], dataset: Dataset) -> dict[str, Any]:
	"""
	The workload, which is empty.

	Raises:
		ValueError: If any parameter is given.
	"""
	if params:
		raise ValueError("ceiling has no workload parameter.")
	return {}

def _measure(
	fio: Fio,
	path: Path,
	name: str,
	pattern: str,
	block: int,
	depth: int,
	size: int = 0
) -> dict[str, Any]:
	params = fio.params
	writes = pattern == 'write'
	options = ['--end_fsync=1', f'--size={size}'] if writes else ['--readonly']
	completed = subprocess.run(
		[
			params['fio'], f'--name={name}', f'--filename={path}', *options,
			f'--rw={pattern}', f'--bs={block}',
			f'--iodepth={depth}', f'--ioengine={params["ioengine"]}',
			f'--direct={int(bool(params["direct"]))}', '--time_based',
			f'--runtime={params["runtime_s"]}', '--output-format=json',
		],
		capture_output=True, text=True, check=False
	)
	if completed.returncode != 0:
		message = (completed.stderr or completed.stdout).strip()
		raise RuntimeError(f"fio failed on {name}: {message}")
	# fio may print notes before its JSON.
	output = completed.stdout
	job = json.loads(output[output.index('{'):])['jobs'][0]
	read = job['write' if writes else 'read']
	percentile = read['clat_ns']['percentile']
	return {
		f'{name}_bytes_per_s': read['bw_bytes'],
		f'{name}_iops': read['iops'],
		f'{name}_latency_p50_s': percentile['50.000000'] / 1e9,
		f'{name}_latency_p99_s': percentile['99.000000'] / 1e9,
	}

def run(
	dataset: Dataset,
	reader: Fio,
	workload: dict[str, Any],
	started: float
) -> dict[str, Any]:
	"""Measure the ceiling of the storage the dataset is on."""
	target = max(dataset.stacks, key=lambda path: path.stat().st_size)
	size = target.stat().st_size
	# No block may be larger than the file, which a small dataset's is.
	largest = size // ALIGNMENT * ALIGNMENT
	if largest == 0:
		raise ValueError(f"{target} is too small to be read directly.")
	block = min(-(-dataset.image_bytes // ALIGNMENT) * ALIGNMENT, largest)
	params = reader.params
	sequential = min(params['sequential_block'], largest)
	metrics = {
		'file_bytes': size,
		'block_bytes': block,
		'sequential_block_bytes': sequential,
		**_measure(
			reader, target, 'sequential', 'read', sequential,
			params['sequential_depth']
		),
	}
	for depth in params['depths']:
		name = f'random_qd{depth}'
		metrics.update(
			_measure(reader, target, name, 'randread', block, depth)
		)
	if params['write_bytes']:
		written = target.parent / f'ceiling-write-{started:.0f}'
		try:
			metrics.update(_measure(
				reader, written, 'sequential_write', 'write', sequential,
				params['sequential_depth'], params['write_bytes']
			))
		finally:
			written.unlink(missing_ok=True)
	return metrics
