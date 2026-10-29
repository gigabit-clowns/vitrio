#!/usr/bin/python3 -I
# SPDX-License-Identifier: LGPL-2.1-or-later

"""What needs root to make a cache cold, as a program of its own.

	vitrio-bench-evict drop
	vitrio-bench-evict zfs
	vitrio-bench-evict arc-max BYTES
	vitrio-bench-evict arc-restore C_MIN C_MAX ZFS_ARC_MIN ZFS_ARC_MAX

`drop` drops every clean page of the machine. `zfs` does that and empties
the ARC of ZFS too, by shrinking it to its floor and giving it back the
limits it had. `arc-max` caps the ARC, and `arc-restore` gives it back the
limits `arc-max` printed. Each prints what it did as JSON, on one line.

The harness runs this as root when it is root, and through `sudo -n`
otherwise, from a copy an administrator owns, as benchmarks/README.md
says. So this file imports nothing of the package, nor anything outside
the standard library, and runs with `-I`, which ignores the environment
and the directory of the script: the copy owned by root never imports from
a path the user can write.

What each platform can do is kept apart, as the system code of the library
is: one implementation per family, chosen by `sys.platform`. Linux does
everything here. macOS drops its caches with `purge`, and has no ARC unless
OpenZFS is installed, whose limits go through `sysctl`, which is not done
here. Windows has no call that drops every cached page; the pages of the
files alone are dropped without privilege, by the harness.
"""

import argparse
import json
import subprocess
import sys
import time

# The least limits ZFS takes for its ARC.
ARC_MIN_FLOOR = 32 << 20
ARC_MAX_FLOOR = 64 << 20

class Error(RuntimeError):
	"""What cannot be done here, and why."""

class _Linux:
	DROP_CACHES = '/proc/sys/vm/drop_caches'
	ARCSTATS = '/proc/spl/kstat/zfs/arcstats'
	ZFS_PARAMETERS = '/sys/module/zfs/parameters'

	@classmethod
	def drop_caches(cls, what):
		try:
			with open(cls.DROP_CACHES, 'w', encoding='ascii') as file:
				file.write(f'{what}\n')
		except OSError as error:
			raise Error(
				f"Writing {cls.DROP_CACHES} needs root: {error}"
			) from error

	@classmethod
	def arcstats(cls):
		try:
			with open(cls.ARCSTATS, encoding='ascii') as file:
				lines = file.read().splitlines()[2:]
		except OSError:
			return None
		stats = {}
		for line in lines:
			fields = line.split()
			if len(fields) == 3 and fields[2].lstrip('-').isdigit():
				stats[fields[0]] = int(fields[2])
		return stats

	@classmethod
	def arc_limits(cls):
		limits = {}
		for name in ('zfs_arc_min', 'zfs_arc_max'):
			try:
				with open(f'{cls.ZFS_PARAMETERS}/{name}', encoding='ascii') as f:
					limits[name] = int(f.read())
			except OSError as error:
				raise Error(f"There is no ARC to limit: {error}") from error
		return limits

	@classmethod
	def set_arc_limit(cls, name, value):
		try:
			with open(f'{cls.ZFS_PARAMETERS}/{name}', 'w', encoding='ascii') as f:
				f.write(f'{value}\n')
		except OSError as error:
			raise Error(
				f"Setting {name} to {value} failed, which needs root and a "
				f"value ZFS takes: {error}"
			) from error

class _Darwin:
	@staticmethod
	def drop_caches(what):
		try:
			subprocess.run(['purge'], check=True, capture_output=True, text=True)
		except (OSError, subprocess.CalledProcessError) as error:
			raise Error(
				f"Dropping the caches of macOS runs purge, as root: {error}"
			) from error

	@staticmethod
	def arcstats():
		return None

	@staticmethod
	def arc_limits():
		raise Error(
			"The ARC of OpenZFS on macOS is limited through sysctl "
			"vfs.zfs.arc.max, which this program does not do."
		)

	@staticmethod
	def set_arc_limit(name, value):
		_Darwin.arc_limits()

class _Windows:
	@staticmethod
	def drop_caches(what):
		raise Error(
			"Windows has no call that drops every cached page. The pages of "
			"the files alone can be dropped, with 'fadvise'."
		)

	@staticmethod
	def arcstats():
		return None

	@staticmethod
	def arc_limits():
		raise Error("There is no ARC on Windows.")

	@staticmethod
	def set_arc_limit(name, value):
		_Windows.arc_limits()

class _Other:
	@staticmethod
	def drop_caches(what):
		raise Error(f"Dropping the caches is not done on {sys.platform}.")

	@staticmethod
	def arcstats():
		return None

	@staticmethod
	def arc_limits():
		raise Error(f"Limiting the ARC is not done on {sys.platform}.")

	@staticmethod
	def set_arc_limit(name, value):
		_Other.arc_limits()

def system():
	"""The implementation of this platform."""
	if sys.platform.startswith('linux'):
		return _Linux
	if sys.platform == 'darwin':
		return _Darwin
	if sys.platform in ('win32', 'cygwin'):
		return _Windows
	return _Other

def drop_caches(what=3):
	"""Drop every clean page (3), or the slabs alone (2). Needs root."""
	system().drop_caches(what)

def read_arcstats():
	"""The statistics of the ARC, by name, or None where there is none."""
	return system().arcstats()

def empty_arc(timeout=30.0):
	"""
	Drop the page cache and empty the ARC: lower its limits to their floor,
	drop the caches, which calls the ARC's shrinker, until it reaches the
	floor or stops shrinking, then give it back the limits it had.

	Returns:
		The bytes the ARC held before, at its floor and once its limits
		were back, and the seconds it took.
	"""
	platform = system()
	stats = platform.arcstats()
	if stats is None:
		raise Error("There is no ARC to empty: ZFS is not loaded.")
	limits = platform.arc_limits()
	if limits['zfs_arc_max'] == ARC_MAX_FLOOR:
		raise Error(
			"zfs_arc_max is at the floor an emptying of the ARC sets, so an "
			"earlier one was interrupted. Give it back its limit first."
		)

	started = time.monotonic()
	try:
		platform.set_arc_limit('zfs_arc_min', ARC_MIN_FLOOR)
		platform.set_arc_limit('zfs_arc_max', ARC_MAX_FLOOR)
		platform.drop_caches(3)
		least = platform.arcstats()['size']
		steady_since = time.monotonic()
		deadline = steady_since + timeout
		while least > 2 * ARC_MAX_FLOOR and time.monotonic() < deadline:
			time.sleep(0.5)
			size = platform.arcstats()['size']
			if size < least * 0.99:
				steady_since = time.monotonic()
			elif time.monotonic() - steady_since > 3.0:
				break
			least = min(least, size)
			platform.drop_caches(2)
	finally:
		restore_limits(
			stats['c_min'], stats['c_max'],
			limits['zfs_arc_min'], limits['zfs_arc_max']
		)
	return {
		'arc_bytes_before': stats['size'],
		'arc_bytes_at_floor': least,
		'arc_bytes_after': platform.arcstats()['size'],
		'seconds': time.monotonic() - started,
	}

def cap_arc(limit):
	"""
	Cap the ARC at `limit` bytes, until `restore_limits` is given back
	what this returns.

	Returns:
		`before`, the limits to give back, and `c_max`, the most the ARC
		may hold now.
	"""
	platform = system()
	stats = platform.arcstats()
	if stats is None:
		raise Error("There is no ARC to cap: ZFS is not loaded.")
	if limit <= ARC_MAX_FLOOR:
		raise Error(
			f"A cap of the ARC is above its floor of {ARC_MAX_FLOOR} bytes."
		)
	limits = platform.arc_limits()
	if limits['zfs_arc_max'] == ARC_MAX_FLOOR:
		raise Error(
			"zfs_arc_max is at the floor an emptying of the ARC sets, so an "
			"earlier one was interrupted. Give it back its limit first."
		)
	before = {
		'c_min': stats['c_min'], 'c_max': stats['c_max'],
		'zfs_arc_min': limits['zfs_arc_min'],
		'zfs_arc_max': limits['zfs_arc_max'],
	}
	# ZFS refuses a maximum below its minimum.
	if stats['c_min'] >= limit:
		platform.set_arc_limit('zfs_arc_min', ARC_MIN_FLOOR)
	platform.set_arc_limit('zfs_arc_max', limit)
	return {'before': before, 'c_max': platform.arcstats()['c_max']}

def restore_limits(c_min, c_max, zfs_arc_min, zfs_arc_max):
	"""
	Give the ARC back its limits: the ones it worked with first, in bytes,
	then the parameters as they were, since a parameter of zero, which
	leaves ZFS its own, may not take effect again by itself.
	"""
	platform = system()
	platform.set_arc_limit('zfs_arc_max', c_max)
	platform.set_arc_limit('zfs_arc_min', c_min)
	platform.set_arc_limit('zfs_arc_max', zfs_arc_max)
	platform.set_arc_limit('zfs_arc_min', zfs_arc_min)
	return {'c_max': platform.arcstats()['c_max']}

def run(command):
	"""
	Carry out a command, given as its words: `['drop']`, `['zfs']`,
	`['arc-max', bytes]` or `['arc-restore', c_min, c_max, zfs_arc_min,
	zfs_arc_max]`.

	Returns:
		What the command reports, or None for `drop`.

	Raises:
		Error: If the command is malformed, or cannot be carried out here.
	"""
	name, *arguments = command
	try:
		numbers = [int(argument) for argument in arguments]
	except ValueError as error:
		raise Error(f"{name} takes numbers of bytes, not {arguments}.") from error
	if any(number < 0 for number in numbers):
		raise Error(f"{name} takes no negative number: {arguments}.")
	if name == 'drop' and not numbers:
		drop_caches(3)
		return None
	if name == 'zfs' and not numbers:
		return empty_arc()
	if name == 'arc-max' and len(numbers) == 1:
		return cap_arc(numbers[0])
	if name == 'arc-restore' and len(numbers) == 4:
		return restore_limits(*numbers)
	raise Error(f"There is no command {command}.")

def main(argv=None):
	parser = argparse.ArgumentParser(
		prog='vitrio-bench-evict',
		description="What needs root to make a cache cold: drop every clean "
		"page, empty the ARC of ZFS, or cap it. Each prints what it did as "
		"JSON."
	)
	commands = parser.add_subparsers(dest='command', required=True)
	commands.add_parser('drop', help="drop every clean page")
	commands.add_parser('zfs', help="drop every clean page and empty the ARC")
	cap = commands.add_parser('arc-max', help="cap the ARC")
	cap.add_argument('bytes')
	restore = commands.add_parser(
		'arc-restore', help="give the ARC back the limits arc-max printed"
	)
	for name in ('c_min', 'c_max', 'zfs_arc_min', 'zfs_arc_max'):
		restore.add_argument(name)
	args = parser.parse_args(argv)
	command = [args.command] + [
		getattr(args, name)
		for name in ('bytes', 'c_min', 'c_max', 'zfs_arc_min', 'zfs_arc_max')
		if hasattr(args, name)
	]
	try:
		print(json.dumps(run(command)))
	except Error as error:
		print(f'vitrio-bench-evict: {error}', file=sys.stderr)
		return 1
	return 0

if __name__ == '__main__':
	sys.exit(main())
