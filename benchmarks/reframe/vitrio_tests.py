# SPDX-License-Identifier: LGPL-2.1-or-later

"""The base tests of the benchmarks, one per procedure.

Each variant of a test is one run of `python -m vitrio_bench run`, in a job
of its own, and its performance is the metrics that run prints. The axes a
sweep may vary are parameters with a single default value here. A named
sweep, in the file of its procedure, gives some of them several values. A
one-off question needs no code: `-P cache=0,64` does the same from the
command line, for parameters and variables alike.

Where the datasets are is the storage being measured, so it has no
default: it is the partition's `datadir` extra, or `-S datadir=...`.
"""

import os
import shlex
import sys
from pathlib import Path

import reframe as rfm
import reframe.utility.sanity as sn
from reframe.core.builtins import (
	parameter,
	performance_function,
	run_after,
	run_before,
	sanity_function,
	variable,
)

HARNESS = Path(__file__).resolve().parents[1] / 'python'
sys.path.insert(0, str(HARNESS))

from vitrio_bench import readers    # noqa: E402

class VitrioBenchmark(rfm.RunOnlyRegressionTest):
	"""
	What every benchmark shares: where the datasets, the results and vitrio
	are, the state of the page cache, the CPUs, and reading the metrics.
	"""

	valid_systems = ['*']
	valid_prog_environs = ['*']
	executable = sys.executable

	#: Where the datasets are generated and read. Defaults to the
	#: partition's `datadir` extra.
	datadir = variable(str, value='')

	#: The file the records are appended to.
	results = variable(
		str, value=os.path.abspath('vitrio-bench-results.jsonl')
	)

	#: A build tree to import vitrio from, instead of the installed one.
	build_tree = variable(str, value='')

	#: How the page cache is emptied before reading: `none`, `fadvise`, or
	#: `drop`, which needs the feature `root`.
	cold = parameter(['none'], type=str)

	#: The CPUs the run is pinned to, such as `0-3`. Empty for all.
	cpus = parameter([''], type=str)

	procedure = ''

	def dataset_options(self):
		"""The `--dataset.*` options of the run."""
		return []

	def run_options(self):
		"""The other options of the run, besides those of the environment."""
		return []

	@run_after('setup')
	def check_capabilities(self):
		partition = self.current_partition
		if not self.datadir:
			self.datadir = partition.extras.get('datadir', '')
		self.skip_if(
			not self.datadir,
			"No place for the datasets: set the partition's 'datadir' extra, "
			"or -S datadir=..."
		)
		self.skip_if(
			self.cold == 'drop' and 'root' not in partition.features,
			"Dropping every cached page needs the feature 'root'."
		)

	@run_before('run')
	def set_command(self):
		paths = [str(HARNESS)]
		if self.build_tree:
			paths.append(str(Path(self.build_tree) / 'python'))
		if os.environ.get('PYTHONPATH'):
			paths.append(os.environ['PYTHONPATH'])
		self.env_vars['PYTHONPATH'] = os.pathsep.join(paths)

		harness = [sys.executable, '-m', 'vitrio_bench']
		dataset = self.dataset_options()
		generate = [*harness, 'generate', '--datadir', self.datadir, *dataset]
		self.prerun_cmds = [shlex.join(generate)]
		environment = [f'--environment.cold={self.cold}']
		if self.cpus:
			environment.append(f'--environment.cpus={self.cpus}')
		self.executable_opts = [
			'-m', 'vitrio_bench', 'run', self.procedure,
			'--datadir', self.datadir, '--results', self.results,
			*dataset, *self.run_options(), *environment,
		]

	@sanity_function
	def succeeded(self):
		return sn.assert_found(r'^vitrio_bench status ok$', self.stdout)

	def metric(self, name):
		"""A metric the run printed."""
		return sn.extractsingle(
			rf'^vitrio_bench metric {name} (\S+)$', self.stdout, 1, float
		)

class ReadBatches(VitrioBenchmark):
	"""Batches read as fast as possible, with no compute (`read_batches`)."""

	procedure = 'read_batches'

	#: Particles in the dataset.
	count = variable(int, value=20000)
	box = parameter([128], type=int)
	dtype = parameter(['float32'], type=str)
	layout = parameter(['single-stack'], type=str)
	seed = variable(int, value=0)

	reader = parameter(['vitrio-loader'], type=str)
	#: Threads of a vitrio reader. 0 takes the reader's default.
	workers = parameter([0], type=int)
	#: Open readers a vitrio reader keeps.
	cache = parameter([0], type=int)
	#: Batches `vitrio-loader` reads ahead.
	in_flight = parameter([2], type=int)

	order = parameter(['shuffled'], type=str)
	batch = parameter([256], type=int)
	#: Particles an epoch reads. 0 reads them all.
	images = variable(int, value=0)
	epochs = variable(int, value=1)

	@run_after('init')
	def check_reader(self):
		needed = readers.missing(readers.get(self.reader))
		self.skip_if(bool(needed), f"{self.reader} needs {needed}.")

	def dataset_options(self):
		return [
			f'--dataset.count={self.count}',
			f'--dataset.box={self.box}',
			f'--dataset.dtype={self.dtype}',
			f'--dataset.layout={self.layout}',
			f'--dataset.seed={self.seed}',
		]

	def run_options(self):
		options = [f'--reader.name={self.reader}']
		# Only the parameters the reader has, since a run refuses others.
		has = readers.get(self.reader).defaults
		if 'workers' in has and self.workers:
			options.append(f'--reader.workers={self.workers}')
		if 'cache' in has:
			options.append(f'--reader.cache={self.cache}')
		if 'in_flight' in has:
			options.append(f'--reader.in_flight={self.in_flight}')
		options += [
			f'--workload.order={self.order}',
			f'--workload.batch={self.batch}',
			f'--workload.epochs={self.epochs}',
		]
		if self.images:
			options.append(f'--workload.images={self.images}')
		return options

	@performance_function('images/s')
	def images_per_s(self):
		return self.metric('images_per_s')

	@performance_function('B/s')
	def file_bytes_per_s(self):
		return self.metric('file_bytes_per_s')

	@performance_function('s')
	def first_batch_s(self):
		return self.metric('first_batch_s')

	@performance_function('s')
	def wait_p99_s(self):
		return self.metric('wait_p99_s')

	@performance_function('s')
	def cpu_per_image_s(self):
		return self.metric('cpu_per_image_s')

	@performance_function('B')
	def peak_memory_bytes(self):
		return self.metric('peak_memory_bytes')

	@performance_function('')
	def read_amplification(self):
		return self.metric('read_amplification')
