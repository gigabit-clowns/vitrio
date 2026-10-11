# SPDX-License-Identifier: LGPL-2.1-or-later

"""The loader reads go through when they are given none.

A dataset only describes its rows. What reads them, a thread pool with its
open readers, is given to each read, and a read given none goes through
this one, shared by the whole process, so that there is a single thread
pool however many datasets there are.
"""

from __future__ import annotations

import os
import threading

from .._binding import ImageLoader
from .._functions import loader as _make_loader

# How many open readers the default loader keeps. The rows of a batch are
# spread over many files, and opening one costs more than reading an image.
DEFAULT_CACHE = 64

_loader: ImageLoader | None = None
_lock = threading.Lock()

# The threads of a loader do not survive a fork, so the child makes a loader
# of its own. The one made before the fork is kept and never used: destroying
# it would wait for threads the child does not have. A child of
# multiprocessing ends without destroying anything.
_orphans: list[ImageLoader] = []

def default_loader() -> ImageLoader:
	"""
	Get the loader shared by the process, made the first time it is asked
	for.

	It reads on as many threads as the machine reports, and keeps
	`DEFAULT_CACHE` open readers.
	"""
	global _loader
	with _lock:
		if _loader is None:
			_loader = _make_loader(cache=DEFAULT_CACHE)
		return _loader

def resolve_loader(loader: ImageLoader | None) -> ImageLoader:
	"""Get the loader given, or the default one when none is."""
	return default_loader() if loader is None else loader

def _forget_in_child() -> None:
	global _loader, _lock
	if _loader is not None:
		_orphans.append(_loader)
	_loader = None
	_lock = threading.Lock()

if hasattr(os, 'register_at_fork'):
	os.register_at_fork(after_in_child=_forget_in_child)
