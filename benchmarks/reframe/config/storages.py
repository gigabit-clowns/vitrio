# SPDX-License-Identifier: LGPL-2.1-or-later

"""ReFrame's configuration for a machine with several storages to measure.

Each storage is a partition of the system `workstation`, so that a campaign
runs on one of them with `--system workstation:<name>`, and on all of them
without `--system`. It is read from the environment, so that one file
serves any machine:

- `VITRIO_BENCH_STORAGES`: the storages, separated by commas, each as
  `<name>=<datadir>`: the name of the partition, and where datasets are
  generated and read on it, which is what is measured. Such as
  `nvme=/scratch/bench,hdd=/data/bench`.
- `VITRIO_BENCH_FEATURES`, `VITRIO_BENCH_SCRATCHDIR`,
  `VITRIO_BENCH_BUILDSDIR`: what every storage has, as `workstation.py`
  reads them.
- `VITRIO_BENCH_<NAME>_FEATURES`, `VITRIO_BENCH_<NAME>_SCRATCHDIR`,
  `VITRIO_BENCH_<NAME>_BUILDSDIR`: what one storage has instead, with its
  name in capitals and `-` as `_`. A storage on ZFS lists `root` for the
  runs that empty its ARC; a storage that is itself the fast one keeps its
  scratch elsewhere.

`-S datadir=...`, `-S scratchdir=...` and `-S buildsdir=...` override them.
"""

import os
import re

def _storages():
	text = os.environ.get('VITRIO_BENCH_STORAGES', '')
	storages = []
	for item in filter(None, (item.strip() for item in text.split(','))):
		name, equals, datadir = item.partition('=')
		if not equals or not datadir or not re.fullmatch(r'[\w-]+', name):
			raise ValueError(
				"VITRIO_BENCH_STORAGES lists the storages as "
				f"<name>=<datadir>, separated by commas, not {item!r}."
			)
		storages.append((name, datadir))
	if not storages:
		raise ValueError(
			"VITRIO_BENCH_STORAGES names no storage. Set it to the storages "
			"to measure, as <name>=<datadir>, separated by commas."
		)
	return storages

def _setting(name, storage):
	own = f"VITRIO_BENCH_{storage.upper().replace('-', '_')}_{name}"
	return os.environ.get(own, os.environ.get(f'VITRIO_BENCH_{name}', ''))

def _partition(name, datadir):
	return {
		'name': name,
		'descr': f'The storage {name}, at {datadir}',
		'scheduler': 'local',
		'launcher': 'local',
		'environs': ['builtin'],
		# One run at a time: runs that overlap compete for the storage
		# they measure.
		'max_jobs': 1,
		'features': [
			feature
			for feature in _setting('FEATURES', name).split(',') if feature
		],
		'extras': {
			'datadir': datadir,
			'scratchdir': _setting('SCRATCHDIR', name),
			'buildsdir': _setting('BUILDSDIR', name),
		},
	}

site_configuration = {
	'systems': [
		{
			'name': 'workstation',
			'descr': 'A workstation with several storages, a partition each',
			'hostnames': ['.*'],
			'partitions': [
				_partition(name, datadir) for name, datadir in _storages()
			],
		},
	],
}
