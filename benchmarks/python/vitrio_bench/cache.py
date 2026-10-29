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
limits, and `arc_capped` holds it under a limit while a run goes, which a
memory limit of the run does not.

What needs root, dropping every page and limiting the ARC, is done by
`evict.py`, in this process when it is root and otherwise through `sudo -n`
and a copy of that program an administrator owns, as benchmarks/README.md
says. What each platform can do is kept apart there, and here for what
needs no privilege: Linux drops the pages of a file with `posix_fadvise`,
Windows by opening it unbuffered, and macOS has no call for one file.
"""

from __future__ import annotations

import contextlib
import ctypes
import json
import os
import shutil
import subprocess
import sys
from collections.abc import Iterator, Sequence
from pathlib import Path

import numpy as np

from . import evict as privileged_program
from .environment import Unavailable, parse_bytes, target

MODES = ('none', 'fadvise', 'drop', 'zfs')

#: The copy of `evict.py` an administrator installs for `sudo -n`.
HELPER = 'vitrio-bench-evict'
HELPER_DIRECTORIES = ('/usr/local/sbin', '/usr/sbin', '/sbin')

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
		_forget_files(paths)
	elif mode == 'drop':
		privileged('drop')
	else:
		return privileged('zfs')
	return None

def _forget_files(paths: Sequence[str | Path]) -> None:
	"""Drop the pages of some files, as this platform does it."""
	if sys.platform in ('win32', 'cygwin'):
		_forget_files_unbuffered(paths)
	elif sys.platform == 'darwin':
		raise Unavailable(
			"macOS has no call that drops the pages of one file. Use 'drop', "
			"which runs purge, as root."
		)
	elif not hasattr(os, 'posix_fadvise'):
		raise Unavailable("This system has no posix_fadvise.")
	else:
		for path in paths:
			descriptor = os.open(path, os.O_RDONLY)
			try:
				os.posix_fadvise(descriptor, 0, 0, os.POSIX_FADV_DONTNEED)
			finally:
				os.close(descriptor)

def _forget_files_unbuffered(paths: Sequence[str | Path]) -> None:
	"""
	Open each file unbuffered and close it, on Windows, where that purges
	what the cache held of a file nothing else has open.
	"""
	kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
	kernel32.CreateFileW.restype = ctypes.c_void_p
	kernel32.CloseHandle.argtypes = (ctypes.c_void_p,)
	generic_read = 0x80000000
	share_read_write = 0x3
	open_existing = 3
	no_buffering = 0x20000000
	invalid = ctypes.c_void_p(-1).value
	for path in paths:
		handle = kernel32.CreateFileW(
			str(path), generic_read, share_read_write, None, open_existing,
			no_buffering, None
		)
		if handle == invalid:
			raise OSError(
				ctypes.get_last_error(), f"Cannot open {path} unbuffered."
			)
		kernel32.CloseHandle(handle)

def _has_privilege() -> bool:
	if hasattr(os, 'geteuid'):
		return os.geteuid() == 0
	try:
		return bool(ctypes.windll.shell32.IsUserAnAdmin())
	except (AttributeError, OSError):
		return False

def _find_helper() -> str | None:
	path = os.pathsep.join([os.environ.get('PATH', ''), *HELPER_DIRECTORIES])
	return shutil.which(HELPER, path=path)

def privileged(*command: str) -> dict | None:
	"""
	Carry out a command of `evict.py`, which needs root: in this process
	when it is root, and otherwise through `sudo -n` and the copy of it an
	administrator installed as `vitrio-bench-evict`.

	Returns:
		What the command reports.

	Raises:
		Unavailable: If neither is possible, or the command cannot be
			carried out here.
	"""
	if _has_privilege():
		try:
			return privileged_program.run(list(command))
		except privileged_program.Error as error:
			raise Unavailable(str(error)) from error
	if sys.platform in ('win32', 'cygwin'):
		raise Unavailable(
			f"{' '.join(command)} needs an administrator: run as one."
		)
	helper = _find_helper()
	sudo = shutil.which('sudo')
	if helper is None or sudo is None:
		raise Unavailable(
			f"{' '.join(command)} needs root, or {HELPER} installed for "
			"sudo -n, as benchmarks/README.md says. Use 'fadvise' instead."
		)
	completed = subprocess.run(
		[sudo, '-n', helper, *command],
		capture_output=True, text=True, timeout=180, check=False
	)
	if completed.returncode != 0:
		raise Unavailable(
			f"{helper} {' '.join(command)} failed: "
			f"{completed.stderr.strip() or completed.stdout.strip()}"
		)
	return json.loads(completed.stdout) if completed.stdout.strip() else None

def arc_size() -> int | None:
	"""The bytes the ARC of ZFS holds, or None where there is none."""
	stats = privileged_program.read_arcstats()
	return stats['size'] if stats else None

def arc_c_max() -> int | None:
	"""The most the ARC of ZFS may hold now, or None where there is none."""
	stats = privileged_program.read_arcstats()
	return stats['c_max'] if stats else None

def warnings(mode: str, storage: dict) -> list[str]:
	"""
	What a cold mode does not do on the storage of a dataset, as
	`environment.target` describes it.
	"""
	if storage.get('filesystem') == 'zfs' and mode in ('fadvise', 'drop'):
		return [
			f"cold={mode} drops nothing of a dataset on ZFS, whose ARC keeps "
			"what it read apart from the page cache; residency is measured "
			"in the page cache alone. Use cold=zfs."
		]
	return []

@contextlib.contextmanager
def arc_capped(
	limit: str | int | None,
	datadir: str | Path
) -> Iterator[dict | None]:
	"""
	Hold the ARC of ZFS under a limit while the body runs, and give it back
	its limits after, whatever the body did.

	Args:
		limit: The most the ARC may hold, in bytes, or as systemd states
			them, such as `8G`. None caps nothing.
		datadir: Where the dataset is, which must be on ZFS.

	Yields:
		What the cap reports: the limits given back later, and `c_max`.

	Raises:
		Unavailable: If the data directory is not on ZFS, or the ARC cannot
			be capped here.
	"""
	if limit is None:
		yield None
		return
	storage = target(datadir)
	if storage.get('filesystem') != 'zfs':
		raise Unavailable(
			"A cap of the ARC applies to a dataset on ZFS, and "
			f"{storage.get('path', datadir)} is on "
			f"{storage.get('filesystem', 'an unknown file system')}."
		)
	capped = privileged('arc-max', str(parse_bytes(limit)))
	try:
		yield capped
	finally:
		before = capped['before']
		privileged(
			'arc-restore', *(str(before[name]) for name in (
				'c_min', 'c_max', 'zfs_arc_min', 'zfs_arc_max'
			))
		)

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
