# SPDX-License-Identifier: LGPL-2.1-or-later

"""The base tests of the benchmarks, one per procedure.

Each variant of a test is one run of `python -m vitrio_bench run`, in a job
of its own, and its performance is the metrics that run prints. The axes a
sweep may vary are parameters with a single default value here. A named
sweep, in the file of its procedure, gives some of them several values. A
one-off question needs no code: `-P cache=0,64` does the same from the
command line, for parameters and variables alike.

Where the datasets are is the storage being measured, so it has no
default: it is the partition's `datadir` extra, or `-S datadir=...`. Where
readers keep copies of a dataset, such as a scratch or a converted
database, is the partition's `scratchdir` extra, or `-S scratchdir=...`.
"""

import json
import os
import shlex
import sys
from pathlib import Path

import reframe as rfm
import reframe.utility.sanity as sn
import reframe.utility.typecheck as typ
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

# The value of a reader's option that leaves it at the reader's default,
# for options whose default differs from one reader to another.
READER_DEFAULT = -1

class VitrioBenchmark(rfm.RunOnlyRegressionTest):
	"""
	What every benchmark shares: where the datasets, the copies, the
	results and vitrio are, the state of the page cache, the CPUs and the
	memory, and reading the metrics.
	"""

	valid_systems = ['*']
	valid_prog_environs = ['*']
	executable = sys.executable

	#: Where the datasets are generated and read. Defaults to the
	#: partition's `datadir` extra.
	datadir = variable(str, value='')

	#: Where readers keep copies of a dataset. Defaults to the partition's
	#: `scratchdir` extra.
	scratchdir = variable(str, value='')

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

	#: The memory the run may hold, page cache included, as systemd states
	#: it, such as `8G`. Empty for no limit. It needs the feature
	#: `cgroup-memory`.
	memory_max = parameter([''], type=str)

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
		if not self.scratchdir:
			self.scratchdir = partition.extras.get('scratchdir', '')
		self.skip_if(
			not self.datadir,
			"No place for the datasets: set the partition's 'datadir' extra, "
			"or -S datadir=..."
		)
		self.skip_if(
			self.cold == 'drop' and 'root' not in partition.features,
			"Dropping every cached page needs the feature 'root'."
		)
		self.skip_if(
			bool(self.memory_max)
			and 'cgroup-memory' not in partition.features,
			"A memory limit needs the feature 'cgroup-memory'."
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
		if self.memory_max:
			environment.append(f'--environment.memory_max={self.memory_max}')
		if self.scratchdir:
			environment.append(f'--environment.scratchdir={self.scratchdir}')
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

class ReaderBenchmark(VitrioBenchmark):
	"""
	What the procedures that read a dataset with a reader share: the
	dataset, the reader and its options, and the workload.
	"""

	#: Particles in the dataset.
	count = variable(int, value=20000)
	box = parameter([128], type=int)
	dtype = parameter(['float32'], type=str)
	layout = parameter(['single-stack'], type=str)
	seed = variable(int, value=0)

	reader = parameter(['vitrio-loader'], type=str)
	#: Threads of a vitrio reader. -1 takes the reader's default.
	workers = parameter([READER_DEFAULT], type=int)
	#: Open readers a vitrio reader keeps. -1 takes the reader's default.
	cache = parameter([READER_DEFAULT], type=int)
	#: Batches `vitrio-loader` reads ahead.
	in_flight = parameter([2], type=int)
	#: Where a vitrio reader keeps copies: `none`, `host`, `mapped`,
	#: `directory`.
	scratch = parameter(['none'], type=str)
	#: Particles of a file a scratch takes in together.
	run_length = parameter([64], type=int)
	#: Batches `vitrio-data` has under way.
	prefetch = parameter([4], type=int)
	#: Worker processes of `torch-dataloader`.
	num_workers = parameter([0], type=int)
	#: Whether the workers of `torch-dataloader` outlive an epoch.
	persistent_workers = variable(typ.Bool, value=False)
	#: Whether `lmdb` reads an earlier conversion rather than converting.
	reuse = parameter([False], type=typ.Bool)
	#: Whether the kernel reads ahead of what `lmdb` touches.
	readahead = parameter([False], type=typ.Bool)

	order = parameter(['shuffled'], type=str)
	batch = parameter([256], type=int)
	#: Particles an epoch reads. 0 reads them all.
	images = variable(int, value=0)
	epochs = variable(int, value=1)

	#: The options of readers the test may pass, under the names the
	#: readers give them.
	READER_OPTIONS = (
		'workers', 'cache', 'in_flight', 'scratch', 'run_length', 'prefetch',
		'num_workers', 'persistent_workers', 'reuse', 'readahead',
	)

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

	def workload_options(self):
		"""The `--workload.*` options of the run."""
		options = [
			f'--workload.order={self.order}',
			f'--workload.batch={self.batch}',
			f'--workload.epochs={self.epochs}',
		]
		if self.images:
			options.append(f'--workload.images={self.images}')
		return options

	def run_options(self):
		options = [f'--reader.name={self.reader}']
		# Only the options the reader has, since a run refuses others.
		has = readers.get(self.reader).defaults
		for name in self.READER_OPTIONS:
			value = getattr(self, name)
			if name not in has:
				continue
			if not isinstance(value, bool) and value == READER_DEFAULT:
				continue
			text = value if isinstance(value, str) else json.dumps(value)
			options.append(f'--reader.{name}={text}')
		return options + self.workload_options()

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

class ReadBatches(ReaderBenchmark):
	"""Batches read as fast as possible, with no compute (`read_batches`)."""

	procedure = 'read_batches'

class Epochs(ReaderBenchmark):
	"""
	Several epochs with a step of known length after each batch
	(`epochs`).
	"""

	procedure = 'epochs'
	epochs = 3

	#: Seconds of the step after each batch. 0 reads as fast as it can.
	step_s = parameter([0.0], type=float)

	def workload_options(self):
		return [*super().workload_options(), f'--workload.step_s={self.step_s}']

	@performance_function('s')
	def first_epoch_s(self):
		return self.metric('first_epoch_s')

	@performance_function('s')
	def later_epoch_s(self):
		return self.metric('later_epoch_s')

	@performance_function('')
	def stall_fraction(self):
		return self.metric('stall_fraction')

	@performance_function('')
	def later_stall_fraction(self):
		return self.metric('later_stall_fraction')

class Ceiling(VitrioBenchmark):
	"""What the storage can deliver, measured with fio (`ceiling`)."""

	procedure = 'ceiling'

	#: The dataset whose largest file fio reads.
	count = variable(int, value=20000)
	box = variable(int, value=128)
	#: Seconds each measurement lasts.
	runtime_s = variable(int, value=10)
	#: fio's engine.
	ioengine = variable(str, value='libaio')

	@run_after('setup')
	def check_fio(self):
		self.skip_if(
			'fio' not in self.current_partition.features,
			"Measuring the ceiling needs the feature 'fio'."
		)

	def dataset_options(self):
		return [f'--dataset.count={self.count}', f'--dataset.box={self.box}']

	def run_options(self):
		return [
			f'--reader.runtime_s={self.runtime_s}',
			f'--reader.ioengine={self.ioengine}',
		]

	@performance_function('B/s')
	def sequential_bytes_per_s(self):
		return self.metric('sequential_bytes_per_s')

	@performance_function('IOPS')
	def random_qd1_iops(self):
		return self.metric('random_qd1_iops')

	@performance_function('IOPS')
	def random_qd16_iops(self):
		return self.metric('random_qd16_iops')

	@performance_function('IOPS')
	def random_qd64_iops(self):
		return self.metric('random_qd64_iops')
