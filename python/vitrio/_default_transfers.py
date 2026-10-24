# SPDX-License-Identifier: LGPL-2.1-or-later

"""The threads the helpers transfer with when they are given none.

One pool of file transfers serves the whole process, so that its threads are
not started anew for every provider. Its conversions run on a thread pool of
its own, apart from the threads of any loader, which may wait for reads it
carries out. That thread pool also does the work of scratches: opening the
files of one being created, filling one ahead of its reads, and copying out
what one has taken in for a read.
"""

from __future__ import annotations

import os
import threading

from ._binding import Executor, FileTransferPool, ThreadPoolExecutor

_executor: ThreadPoolExecutor | None = None
_pool: FileTransferPool | None = None
_lock = threading.Lock()

# The threads of a pool do not survive a fork, so the child makes pools of
# its own. Those made before the fork are kept and never used: destroying
# them would wait for threads the child does not have.
_orphans: list[ThreadPoolExecutor | FileTransferPool] = []

def _get_executor() -> ThreadPoolExecutor:
	global _executor
	if _executor is None:
		_executor = ThreadPoolExecutor(os.cpu_count() or 1)
	return _executor

def default_executor() -> Executor:
	"""
	Get the thread pool shared by the process for the work of transfers,
	made the first time it is asked for.

	It has as many threads as the machine reports. None of its work waits
	for another thread of it.
	"""
	with _lock:
		return _get_executor()

def default_file_transfer_pool() -> FileTransferPool:
	"""
	Get the pool of file transfers shared by the process, made the first
	time it is asked for.

	It converts on `default_executor`.
	"""
	global _pool
	with _lock:
		if _pool is None:
			_pool = FileTransferPool(_get_executor())
		return _pool

def _forget_in_child() -> None:
	global _executor, _pool, _lock
	for orphan in (_pool, _executor):
		if orphan is not None:
			_orphans.append(orphan)
	_executor = None
	_pool = None
	_lock = threading.Lock()

if hasattr(os, 'register_at_fork'):
	os.register_at_fork(after_in_child=_forget_in_child)
