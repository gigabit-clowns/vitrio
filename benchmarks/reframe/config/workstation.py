# SPDX-License-Identifier: LGPL-2.1-or-later

"""ReFrame's configuration for a workstation that runs the benchmarks itself.

Copy it, and fit two things to the machine:

- `features`: what the machine can do that a benchmark may ask for.
  `root` drops every cached page, `cgroup-memory` limits the memory of a
  run, `fio` measures the storage's ceiling, `gpu` takes batches to a GPU.
  A variant that asks for what is not listed is skipped, and says why.
- `datadir`: where datasets are generated and read, which is the storage
  being measured. It is read from `VITRIO_BENCH_DATADIR` here, or given
  with `-S datadir=...`.
"""

import os

site_configuration = {
	'systems': [
		{
			'name': 'workstation',
			'descr': 'A workstation that runs the benchmarks itself',
			'hostnames': ['.*'],
			'partitions': [
				{
					'name': 'local',
					'descr': 'The machine ReFrame runs on',
					'scheduler': 'local',
					'launcher': 'local',
					'environs': ['builtin'],
					# One run at a time: runs that overlap compete for the
					# storage they measure.
					'max_jobs': 1,
					'features': [],
					'extras': {
						'datadir': os.environ.get('VITRIO_BENCH_DATADIR', ''),
					},
				},
			],
		},
	],
}
