# SPDX-License-Identifier: LGPL-2.1-or-later

"""What a run measures about itself, from outside vitrio.

vitrio has no counters of its own, so CPU time, memory and the bytes read
from storage are taken from the operating system, for the whole process
tree: the worker processes some baselines start count as much as vitrio's
threads.
"""

from __future__ import annotations

import threading
from collections.abc import Sequence
from pathlib import Path
from typing import Any

import numpy as np
import psutil

def _memory(process: psutil.Process) -> tuple[int, str]:
	# PSS shares the pages several processes map between them, so that a
	# tree of processes is not counted twice. Only Linux has it.
	info = process.memory_full_info()
	for kind in ('pss', 'uss', 'rss'):
		if hasattr(info, kind):
			return getattr(info, kind), kind
	return info.rss, 'rss'

def _read_bytes(process: psutil.Process) -> int | None:
	try:
		return process.io_counters().read_bytes
	except (AttributeError, psutil.AccessDenied):
		return None

class ProcessTree:
	"""
	Measures the CPU time, the memory and the bytes read of this process
	and of every process it starts, while a `with` block runs.

	Memory is sampled in a thread of its own, so its peak is that of the
	samples. CPU time and bytes read are taken at both ends. The bytes read
	by a child are its last sample, since a child that has ended takes
	its counters with it.
	"""

	def __init__(self, interval: float = 0.25) -> None:
		self.interval = interval
		self.peak_memory_bytes = 0
		self.memory_kind: str | None = None
		self.cpu_s: float | None = None
		self.read_bytes: int | None = None
		self._process = psutil.Process()
		self._children: dict[int, int] = {}
		self._stop = threading.Event()
		self._thread: threading.Thread | None = None

	def _cpu(self) -> float:
		times = self._process.cpu_times()
		total = times.user + times.system
		# Children that have ended and been waited for.
		total += getattr(times, 'children_user', 0.0)
		total += getattr(times, 'children_system', 0.0)
		for child in self._process.children(recursive=True):
			try:
				child_times = child.cpu_times()
				total += child_times.user + child_times.system
			except psutil.NoSuchProcess:
				pass
		return total

	def _sample(self) -> None:
		memory, kind = _memory(self._process)
		for child in self._process.children(recursive=True):
			try:
				child_memory, _ = _memory(child)
				memory += child_memory
				read = _read_bytes(child)
				if read is not None:
					self._children[child.pid] = read
			except psutil.NoSuchProcess:
				pass
		self.memory_kind = kind
		self.peak_memory_bytes = max(self.peak_memory_bytes, memory)

	def _run(self) -> None:
		while not self._stop.wait(self.interval):
			self._sample()

	def __enter__(self) -> ProcessTree:
		self._cpu_start = self._cpu()
		self._read_start = _read_bytes(self._process)
		self._sample()
		self._thread = threading.Thread(target=self._run, daemon=True)
		self._thread.start()
		return self

	def __exit__(self, *_: object) -> None:
		self._stop.set()
		self._thread.join()
		self._sample()
		self.cpu_s = self._cpu() - self._cpu_start
		read_end = _read_bytes(self._process)
		if read_end is not None and self._read_start is not None:
			self.read_bytes = (
				read_end - self._read_start + sum(self._children.values())
			)

	def metrics(self) -> dict[str, Any]:
		"""What was measured, named as the records name it."""
		return {
			'cpu_s': self.cpu_s,
			'peak_memory_bytes': self.peak_memory_bytes,
			'memory_kind': self.memory_kind,
			'read_bytes': self.read_bytes,
		}

def max_rss_bytes() -> int | None:
	"""
	The largest resident set this process has had, exactly, unlike the
	sampled peak of `ProcessTree`.
	"""
	try:
		import resource
		import sys
	except ImportError:
		return None
	peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
	# Linux counts it in KiB, macOS in bytes.
	return peak if sys.platform == 'darwin' else peak * 1024

def nfs_read_bytes(mountpoint: str | Path) -> int | None:
	"""
	The bytes this process has read from the NFS server of a mount, from
	`/proc/self/mountstats`. None if the mount is not NFS or the system
	does not say.
	"""
	try:
		with open('/proc/self/mountstats', encoding='utf-8') as file:
			lines = file.read().splitlines()
	except OSError:
		return None
	inside = False
	for line in lines:
		if line.startswith('device '):
			words = line.split()
			inside = (
				len(words) > 7 and words[4] == str(mountpoint)
				and words[7].startswith('nfs')
			)
		elif inside and line.strip().startswith('bytes:'):
			# normal read, normal write, direct read, direct write,
			# server read, server write, pages read, pages written
			return int(line.split()[5])
	return None

def percentiles(
	values: Sequence[float],
	prefix: str,
	points: Sequence[int] = (50, 90, 99)
) -> dict[str, float | None]:
	"""Percentiles of some values, named `<prefix>_p<point>_s`."""
	if not values:
		return {f'{prefix}_p{point}_s': None for point in points}
	found = np.percentile(np.asarray(values), points)
	return {
		f'{prefix}_p{point}_s': float(value)
		for point, value in zip(points, found)
	}
