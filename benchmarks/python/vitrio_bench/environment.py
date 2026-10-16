# SPDX-License-Identifier: LGPL-2.1-or-later

"""What a run happens on: the machine, where the data is, and which vitrio.

The machine is described once and named by a hash of its description, which
leaves out whatever changes from one run to the next. Runs refer to it by
that name.
"""

from __future__ import annotations

import hashlib
import importlib.metadata
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import Any

import psutil

# The packages whose versions a record states, besides vitrio. They are
# looked up without being imported, which for PyTorch takes seconds.
SOFTWARE = ('numpy', 'psutil', 'mrcfile', 'torch', 'reframe-hpc')

class Unavailable(RuntimeError):
	"""A capability was asked for, and the machine does not have it."""

def _cpu_model() -> str:
	try:
		with open('/proc/cpuinfo', encoding='utf-8') as file:
			for line in file:
				if line.startswith('model name'):
					return line.split(':', 1)[1].strip()
	except OSError:
		pass
	return platform.processor()

def _gpus() -> list[str]:
	if shutil.which('nvidia-smi') is None:
		return []
	try:
		output = subprocess.run(
			[
				'nvidia-smi', '--query-gpu=name,memory.total',
				'--format=csv,noheader',
			],
			capture_output=True, text=True, timeout=30, check=True
		).stdout
	except (OSError, subprocess.SubprocessError):
		return []
	return [line.strip() for line in output.splitlines() if line.strip()]

def machine() -> dict[str, Any]:
	"""What the machine is, leaving out what changes between runs."""
	return {
		'system': platform.system(),
		'release': platform.release(),
		'architecture': platform.machine(),
		'cpu': _cpu_model(),
		'logical_cpus': os.cpu_count(),
		'memory_bytes': psutil.virtual_memory().total,
		'gpus': _gpus(),
	}

def machine_id(description: dict[str, Any]) -> str:
	"""The name of a machine: a hash of its description."""
	text = json.dumps(description, sort_keys=True)
	return hashlib.sha256(text.encode()).hexdigest()[:16]

def save_machine(directory: str | Path) -> str:
	"""
	Describe the machine into `<directory>/machines/<name>.json`, unless it
	is described there already.

	Returns:
		str: The name of the machine.
	"""
	description = machine()
	name = machine_id(description)
	path = Path(directory) / 'machines' / f'{name}.json'
	if not path.exists():
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(json.dumps(description, indent='\t', sort_keys=True))
	return name

def target(path: str | Path) -> dict[str, Any]:
	"""
	The file system a path is on: what it is, what holds it, how it is
	mounted.
	"""
	path = Path(path).resolve()
	best = None
	for partition in psutil.disk_partitions(all=True):
		mountpoint = Path(partition.mountpoint)
		if path == mountpoint or mountpoint in path.parents:
			if best is None or len(str(mountpoint)) > len(best.mountpoint):
				best = partition
	if best is None:
		return {'path': str(path)}
	return {
		'path': str(path),
		'mountpoint': best.mountpoint,
		'filesystem': best.fstype,
		'device': best.device,
		'options': best.opts,
	}

def _find_cmake_cache(directory: Path) -> Path | None:
	for parent in (directory, *directory.parents):
		candidate = parent / 'CMakeCache.txt'
		if candidate.is_file():
			return candidate
	return None

def _read_cmake_cache(path: Path) -> dict[str, str]:
	entries = {}
	with open(path, encoding='utf-8') as file:
		for line in file:
			if line.startswith(('#', '//')) or '=' not in line:
				continue
			key, value = line.rstrip('\n').split('=', 1)
			entries[key.split(':', 1)[0]] = value
	return entries

def _git(directory: str, *arguments: str) -> str | None:
	try:
		return subprocess.run(
			['git', '-C', directory, *arguments],
			capture_output=True, text=True, timeout=30, check=True
		).stdout.strip()
	except (OSError, subprocess.SubprocessError):
		return None

def _version(package: str) -> str | None:
	try:
		return importlib.metadata.version(package)
	except importlib.metadata.PackageNotFoundError:
		return None

def build() -> dict[str, Any]:
	"""
	Which vitrio a run imports, and the versions of what runs beside it.

	A vitrio imported from a build tree is described by that tree's CMake
	cache: its build type, its compiler flags, which carry any tuning macro
	the build overrides, and the commit of its sources. An installed one is
	described by its version alone.
	"""
	import vitrio as vio

	package = Path(vio.__file__).resolve().parent
	record: dict[str, Any] = {
		'vitrio': vio.__version__,
		'package': str(package),
		'python': platform.python_version(),
		'software': {name: _version(name) for name in SOFTWARE},
	}
	cache = _find_cmake_cache(package)
	if cache is None:
		record['kind'] = 'installed'
		return record

	entries = _read_cmake_cache(cache)
	sources = entries.get('CMAKE_HOME_DIRECTORY', '')
	status = _git(sources, 'status', '--porcelain') if sources else None
	record.update({
		'kind': 'build-tree',
		'tree': str(cache.parent),
		'build_type': entries.get('CMAKE_BUILD_TYPE'),
		'cxx_flags': entries.get('CMAKE_CXX_FLAGS'),
		'commit': _git(sources, 'rev-parse', 'HEAD') if sources else None,
		'dirty': bool(status) if status is not None else None,
	})
	return record

def set_cpus(cpus: str) -> list[int]:
	"""
	Run this process, and whatever it starts, on the CPUs a list such as
	`0-3,8` names.

	It has to happen before vitrio starts any thread.

	Raises:
		Unavailable: If the system cannot pin a process to CPUs.
	"""
	if not hasattr(os, 'sched_setaffinity'):
		raise Unavailable(f"{platform.system()} cannot pin a process to CPUs.")
	chosen = set()
	for part in cpus.split(','):
		first, _, last = part.partition('-')
		chosen.update(range(int(first), int(last or first) + 1))
	os.sched_setaffinity(0, chosen)
	return sorted(chosen)

def raise_open_file_limit() -> None:
	"""
	Allow this process as many open files as the system lets it have.

	Readers that keep every stack open, and large caches of open readers,
	would otherwise run out at 1024.
	"""
	try:
		import resource
	except ImportError:
		return
	soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
	if soft < hard:
		resource.setrlimit(resource.RLIMIT_NOFILE, (hard, hard))
