# SPDX-License-Identifier: LGPL-2.1-or-later

"""The pool of file transfers a reader provider uses when it is given none.

One pool serves the whole process, so that its threads are not started anew
for every provider. Its conversions run on a thread pool of its own, apart
from the threads of any loader, which may wait for reads it carries out.
"""

from __future__ import annotations

import os
import threading

from ._binding import FileTransferPool, ThreadPoolExecutor

_pool: FileTransferPool | None = None
_lock = threading.Lock()

# The threads of a pool do not survive a fork, so the child makes a pool of
# its own. The one made before the fork is kept and never used: destroying it
# would wait for threads the child does not have.
_orphans: list[FileTransferPool] = []

def default_file_transfer_pool() -> FileTransferPool:
	"""
	Get the pool of file transfers shared by the process, made the first
	time it is asked for.

	It converts on as many threads as the machine reports.
	"""
	global _pool
	with _lock:
		if _pool is None:
			_pool = FileTransferPool(ThreadPoolExecutor(os.cpu_count() or 1))
		return _pool

def _forget_in_child() -> None:
	global _pool, _lock
	if _pool is not None:
		_orphans.append(_pool)
	_pool = None
	_lock = threading.Lock()

if hasattr(os, 'register_at_fork'):
	os.register_at_fork(after_in_child=_forget_in_child)
