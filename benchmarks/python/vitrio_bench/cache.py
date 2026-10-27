# SPDX-License-Identifier: LGPL-2.1-or-later

"""Making the page cache forget a dataset, and checking that it did.

A cold run is only cold if the pages of its files are not in memory when it
starts. Asking the kernel to drop them is not enough to know that, so the
residency of the files is measured afterwards, through `mincore`, and
recorded with the run.

On NFS this empties the client's cache only. The server keeps its own, which
no client can drop.

ZFS keeps what it reads in its ARC, apart from the page cache, and leaves it
there whatever `POSIX_FADV_DONTNEED` or `drop_caches` ask. The mode `zfs`
empties the ARC too, by shrinking it to its floor and giving it back its
limits.
"""

from __future__ import annotations

import ctypes
import os
import time
from collections.abc import Sequence
from pathlib import Path

import numpy as np

from .environment import Unavailable

MODES = ('none', 'fadvise', 'drop', 'zfs')

DROP_CACHES = Path('/proc/sys/vm/drop_caches')
ARCSTATS = Path('/proc/spl/kstat/zfs/arcstats')
ZFS_PARAMETERS = Path('/sys/module/zfs/parameters')
# The least limits ZFS takes for its ARC.
ARC_MIN_FLOOR = 32 << 20
ARC_MAX_FLOOR = 64 << 20

def evict(paths: Sequence[str | Path], mode: str) -> dict | None:
	"""
	Ask the kernel to drop the cached pages of some files.

	Args:
		paths: The files.
		mode: `'fadvise'` drops the pages of those files alone, with
			`POSIX_FADV_DONTNEED`, and needs no privilege. `'drop'` drops
			every clean page of the machine, through
			`/proc/sys/vm/drop_caches`, and needs root. `'zfs'` does what
			`'drop'` does and empties the ARC of ZFS too, which needs root
			as well. `'none'` does nothing.

	Returns:
		For `'zfs'`, the bytes the ARC held before and at its floor, and the
		seconds emptying it took. Nothing for the other modes.

	Raises:
		Unavailable: If the system cannot do what `mode` asks.
		ValueError: If `mode` is none of `MODES`.
	"""
	if mode not in MODES:
		raise ValueError(
			f"A cache is made cold by one of {MODES}, not {mode!r}."
		)
	if mode == 'none':
		return

	# Only clean pages can be dropped, so whatever was just written is
	# written out first.
	if hasattr(os, 'sync'):
		os.sync()

	if mode == 'fadvise':
		if not hasattr(os, 'posix_fadvise'):
			raise Unavailable("This system has no posix_fadvise.")
		for path in paths:
			descriptor = os.open(path, os.O_RDONLY)
			try:
				os.posix_fadvise(descriptor, 0, 0, os.POSIX_FADV_DONTNEED)
			finally:
				os.close(descriptor)
	elif mode == 'drop':
		_drop_caches(3)
	else:
		return _empty_arc()
	return None

def _drop_caches(what: int) -> None:
	try:
		DROP_CACHES.write_text(f'{what}\n', encoding='ascii')
	except OSError as error:
		raise Unavailable(
			"Dropping every cached page needs root. Use 'fadvise' instead."
		) from error

def _read_arcstats() -> dict[str, int] | None:
	try:
		lines = ARCSTATS.read_text(encoding='ascii').splitlines()[2:]
	except OSError:
		return None
	stats = {}
	for line in lines:
		fields = line.split()
		if len(fields) == 3 and fields[2].lstrip('-').isdigit():
			stats[fields[0]] = int(fields[2])
	return stats

def _write_parameter(name: str, value: int) -> None:
	(ZFS_PARAMETERS / name).write_text(f'{value}\n', encoding='ascii')

def _empty_arc(timeout: float = 30.0) -> dict:
	"""
	Drop the page cache and empty the ARC: lower its limits to their floor,
	drop the caches, which calls the ARC's shrinker, until it reaches the
	floor or stops shrinking, then give it back the limits it had.
	"""
	stats = _read_arcstats()
	if stats is None:
		raise Unavailable("There is no ARC to empty: ZFS is not loaded.")
	parameters = {
		name: int((ZFS_PARAMETERS / name).read_text(encoding='ascii'))
		for name in ('zfs_arc_min', 'zfs_arc_max')
	}
	if parameters['zfs_arc_max'] == ARC_MAX_FLOOR:
		raise Unavailable(
			"zfs_arc_max is at the floor an emptying of the ARC sets, so an "
			"earlier one was interrupted. Give it back its limit first."
		)
	if not os.access(ZFS_PARAMETERS / 'zfs_arc_max', os.W_OK):
		raise Unavailable("Emptying the ARC needs root.")

	started = time.monotonic()
	try:
		_write_parameter('zfs_arc_min', ARC_MIN_FLOOR)
		_write_parameter('zfs_arc_max', ARC_MAX_FLOOR)
		_drop_caches(3)
		least = _read_arcstats()['size']
		steady_since = time.monotonic()
		deadline = steady_since + timeout
		while least > 2 * ARC_MAX_FLOOR and time.monotonic() < deadline:
			time.sleep(0.5)
			size = _read_arcstats()['size']
			if size < least * 0.99:
				steady_since = time.monotonic()
			elif time.monotonic() - steady_since > 3.0:
				break
			least = min(least, size)
			_drop_caches(2)
	finally:
		# The limits it worked with first, in bytes: a parameter of zero,
		# which leaves ZFS its own, may not take effect again by itself.
		_write_parameter('zfs_arc_max', stats['c_max'])
		_write_parameter('zfs_arc_min', stats['c_min'])
		_write_parameter('zfs_arc_max', parameters['zfs_arc_max'])
		_write_parameter('zfs_arc_min', parameters['zfs_arc_min'])
	return {
		'arc_bytes_before': stats['size'],
		'arc_bytes_at_floor': least,
		'seconds': time.monotonic() - started,
	}

def _load_libc():
	try:
		libc = ctypes.CDLL(None, use_errno=True)
		mmap, mincore, munmap = libc.mmap, libc.mincore, libc.munmap
	except (OSError, AttributeError, TypeError):
		return None
	mmap.restype = ctypes.c_void_p
	mmap.argtypes = (
		ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_int,
		ctypes.c_int, ctypes.c_int64,
	)
	mincore.restype = ctypes.c_int
	mincore.argtypes = (
		ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_ubyte),
	)
	munmap.restype = ctypes.c_int
	munmap.argtypes = (ctypes.c_void_p, ctypes.c_size_t)
	return mmap, mincore, munmap

_LIBC = _load_libc()
_PROT_READ = 1
_MAP_SHARED = 1
_MAP_FAILED = ctypes.c_void_p(-1).value

def _resident_pages(path: str | Path) -> tuple[int, int]:
	mmap, mincore, munmap = _LIBC
	size = os.path.getsize(path)
	page = os.sysconf('SC_PAGESIZE')
	pages = (size + page - 1) // page
	if pages == 0:
		return 0, 0

	descriptor = os.open(path, os.O_RDONLY)
	try:
		address = mmap(None, size, _PROT_READ, _MAP_SHARED, descriptor, 0)
	finally:
		os.close(descriptor)
	if address in (None, _MAP_FAILED):
		raise OSError(ctypes.get_errno(), f"Cannot map {path}.")
	try:
		vector = (ctypes.c_ubyte * pages)()
		if mincore(address, size, vector) != 0:
			raise OSError(ctypes.get_errno(), f"mincore failed on {path}.")
		resident = int(np.count_nonzero(np.frombuffer(vector, np.uint8) & 1))
	finally:
		munmap(address, size)
	return resident, pages

def residency(paths: Sequence[str | Path], sample: int = 64) -> float | None:
	"""
	The fraction of the pages of some files that are in memory.

	Args:
		paths: The files.
		sample: How many of them to look at, spread evenly over the list,
			so that a dataset of thousands of files is checked quickly.

	Returns:
		float | None: From 0, nothing cached, to 1, all of it. None where
		the system has no `mincore`.
	"""
	if _LIBC is None or not hasattr(os, 'sysconf') or not paths:
		return None
	step = max(1, len(paths) // sample)
	resident = total = 0
	for path in paths[::step]:
		pages_in, pages = _resident_pages(path)
		resident += pages_in
		total += pages
	return resident / total if total else 0.0
