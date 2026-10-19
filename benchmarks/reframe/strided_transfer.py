# SPDX-License-Identifier: LGPL-2.1-or-later

"""The benchmark of the engine that moves images between a file and an array.

It is a program of Google Benchmark, built in a build tree configured with
`-DVITRIO_BUILD_BENCHMARKS=ON`, which `-S build_tree=...` names. Its results
are Google Benchmark's own JSON, written next to the results file, and the
median throughput of each benchmark is a performance variable of this test.
Two runs compare with Google Benchmark's `compare.py`.
"""

import json
import os
import shlex
import time
from pathlib import Path

import reframe as rfm
import reframe.utility.sanity as sn
from reframe.core.builtins import (
	run_after,
	run_before,
	sanity_function,
	variable,
)

BINARY = Path('benchmarks') / 'cpp' / 'vitrio-benchmark-strided-transfer'
OUTPUT = 'strided_transfer.json'

@rfm.simple_test
class strided_transfer(rfm.RunOnlyRegressionTest):
	"""
	Every pair of types, contiguous and strided, and a std::memcpy of the
	same images as the ceiling.
	"""

	valid_systems = ['*']
	valid_prog_environs = ['*']
	executable = 'false'

	#: The build tree the benchmark was built in.
	build_tree = variable(str, value='')

	#: The file of records, next to which Google Benchmark's JSON is kept.
	results = variable(
		str, value=os.path.abspath('vitrio-bench-results.jsonl')
	)

	#: Runs of each benchmark, of which the median is reported.
	repetitions = variable(int, value=3)

	#: Which benchmarks to run, as `--benchmark_filter` selects them.
	filter = variable(str, value='')

	@run_after('setup')
	def find_binary(self):
		self.skip_if(
			not self.build_tree,
			"Name the build tree the benchmark was built in: "
			"-S build_tree=..."
		)
		binary = Path(self.build_tree) / BINARY
		self.skip_if(
			not binary.is_file(),
			f"{binary} is not built: configure with "
			"-DVITRIO_BUILD_BENCHMARKS=ON."
		)
		self.executable = str(binary)

	@run_before('run')
	def set_options(self):
		self.executable_opts = [
			f'--benchmark_repetitions={self.repetitions}',
			'--benchmark_report_aggregates_only=true',
			f'--benchmark_out={OUTPUT}',
			'--benchmark_out_format=json',
		]
		if self.filter:
			self.executable_opts.append(f'--benchmark_filter={self.filter}')
		# ReFrame writes the options into a shell script as they are, where
		# the | of a filter would be a pipe.
		self.executable_opts = [shlex.quote(o) for o in self.executable_opts]
		directory = Path(self.results).parent
		kept = directory / (
			f"strided-transfer-{time.strftime('%Y%m%dT%H%M%S')}.json"
		)
		self.postrun_cmds = [
			shlex.join(['mkdir', '-p', str(directory)]),
			shlex.join(['cp', OUTPUT, str(kept)]),
		]

	def _benchmarks(self):
		path = os.path.join(self.stagedir, OUTPUT)
		with open(path, encoding='utf-8') as file:
			return json.load(file)['benchmarks']

	@sanity_function
	def ran(self):
		return sn.assert_found(r'"benchmarks"', OUTPUT)

	@run_before('performance')
	def set_performance(self):
		for benchmark in self._benchmarks():
			if benchmark.get('aggregate_name') != 'median':
				continue
			if 'bytes_per_second' in benchmark:
				value, unit = benchmark['bytes_per_second'], 'B/s'
			else:
				value, unit = benchmark['images_per_second'], 'images/s'
			self.perf_variables[benchmark['run_name']] = (
				sn.make_performance_function(sn.defer(value), unit)
			)
