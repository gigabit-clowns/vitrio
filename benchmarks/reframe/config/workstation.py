# SPDX-License-Identifier: LGPL-2.1-or-later

"""ReFrame's configuration for a workstation that runs the benchmarks itself.

It is read from the environment, so that one file serves any machine:

- `VITRIO_BENCH_FEATURES`: what the machine can do that a benchmark may ask
  for, separated by commas. `root` drops every cached page,
  `cgroup-memory` limits the memory of a run, `fio` measures the storage's
  ceiling. A variant that asks for what is not listed is skipped, and says
  why.
- `VITRIO_BENCH_DATADIR`: where datasets are generated and read, which is
  the storage being measured.
- `VITRIO_BENCH_SCRATCHDIR`: where readers keep copies of a dataset, such
  as a scratch or a converted database: fast local storage, as a rule.
- `VITRIO_BENCH_BUILDSDIR`: where the builds of vitrio that sweeps of its
  tuning macros compare are kept, between campaigns. Defaults to
  build/benchmarks of the sources.

`-S datadir=...`, `-S scratchdir=...` and `-S buildsdir=...` override the
last three.
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
					'features': [
						feature for feature in os.environ.get(
							'VITRIO_BENCH_FEATURES', ''
						).split(',') if feature
					],
					'extras': {
						'datadir': os.environ.get('VITRIO_BENCH_DATADIR', ''),
						'scratchdir': os.environ.get(
							'VITRIO_BENCH_SCRATCHDIR', ''
						),
						'buildsdir': os.environ.get(
							'VITRIO_BENCH_BUILDSDIR', ''
						),
					},
				},
			],
		},
	],
}
