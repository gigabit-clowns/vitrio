# SPDX-License-Identifier: LGPL-2.1-or-later

"""The command line of the harness.

	python -m vitrio_bench env
	python -m vitrio_bench generate --datadir DIR [--dataset.NAME=VALUE ...]
	python -m vitrio_bench cold {none,fadvise,drop} PATH...
	python -m vitrio_bench dataset list
	python -m vitrio_bench dataset check NAME ROOT
	python -m vitrio_bench run PROCEDURE --datadir DIR --results FILE \\
		[--PART.NAME=VALUE ...]

`run` reads one point of a sweep and appends its record to the results
file. A point is given part by part, `--reader.cache=64`, and a value is
read as JSON when it is valid JSON (`64`, `true`, `null`) and as text
otherwise (`shuffled`). Whatever is not given takes its default, which the
record writes out. A name a part does not have is an error rather than
ignored, so that a typing mistake does not measure the default.

The run prints its status and its metrics, one per line, for whoever
launched it:

	vitrio_bench status ok
	vitrio_bench metric images_per_s 12345.6
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import traceback
from pathlib import Path
from typing import Any

import psutil

from . import cache, datasets, environment, manifests, procedures, readers
from . import results
from .metrics import nfs_read_bytes

PREFIX = 'vitrio_bench'
RUN_PARTS = ('dataset', 'reader', 'workload', 'environment')
ENVIRONMENT = {
	'cold': 'none',
	'cpus': None,
	'memory_max': None,
	'scratchdir': None,
}

# What a run started within a memory limit is told by the run that started
# it: that it is within the limit, when the first process began, and the
# identifier of the record it is to write.
SCOPED = 'VITRIO_BENCH_SCOPED'
STARTED = 'VITRIO_BENCH_STARTED'
RECORD = 'VITRIO_BENCH_RECORD'

def _value(text: str) -> Any:
	try:
		return json.loads(text)
	except json.JSONDecodeError:
		return text

def parse_parts(
	arguments: list[str],
	parts: tuple[str, ...]
) -> dict[str, dict[str, Any]]:
	"""
	Gather `--<part>.<name>=<value>` arguments, part by part.

	Raises:
		ValueError: If an argument is not of that form, or names a part
			that is not among `parts`.
	"""
	gathered: dict[str, dict[str, Any]] = {part: {} for part in parts}
	for argument in arguments:
		key, equals, text = argument.partition('=')
		part, dot, name = key.removeprefix('--').partition('.')
		if not key.startswith('--') or not equals or not dot or not name:
			raise ValueError(
				f"{argument!r} is not of the form --<part>.<name>=<value>."
			)
		if part not in parts:
			raise ValueError(f"There is no part {part!r}. There are {parts}.")
		gathered[part][name] = _value(text)
	return gathered

def _environment(params: dict[str, Any]) -> dict[str, Any]:
	unknown = set(params) - set(ENVIRONMENT)
	if unknown:
		raise ValueError(
			f"The environment has no parameter {sorted(unknown)}. It has "
			f"{sorted(ENVIRONMENT)}."
		)
	return {**ENVIRONMENT, **params}

def _report(record: dict[str, Any]) -> None:
	print(f"{PREFIX} status {record['status']}")
	if record['reason']:
		print(f"{PREFIX} reason {record['reason']}")
	for name, value in sorted(record['metrics'].items()):
		# What could not be measured here prints as nan, so that whoever
		# reads the metrics finds every line it expects.
		if value is None:
			value = float('nan')
		if isinstance(value, (int, float)) and not isinstance(value, bool):
			print(f"{PREFIX} metric {name} {value}")

def _finish(results_file: str, record: dict[str, Any]) -> int:
	record['finished'] = results.now()
	results.append(results_file, record)
	_report(record)
	return 1 if record['status'] == 'failed' else 0

def _recorded(results_file: str, identifier: str) -> bool:
	try:
		with open(results_file, encoding='utf-8') as file:
			return any(identifier in line for line in file)
	except OSError:
		return False

def _within_memory_limit(
	args: argparse.Namespace,
	parts: dict[str, dict[str, Any]],
	memory_max: str
) -> int:
	"""
	Run the same point again in a scope of systemd whose memory, page cache
	included, is limited, and record what the limit did to it.

	A run the limit kills cannot record itself, so this process records it
	instead, as failed.
	"""
	record = results.new_record(args.procedure)
	for part in RUN_PARTS:
		record[part] = dict(parts[part])
	command = [
		'systemd-run', '--user', '--scope', '--quiet',
		'-p', f'MemoryMax={memory_max}', '-p', 'MemorySwapMax=0', '--',
		sys.executable, '-m', 'vitrio_bench', *args.argv,
	]
	if shutil.which('systemd-run') is None:
		record['status'] = 'failed'
		record['reason'] = "A memory limit needs systemd-run."
		return _finish(args.results, record)

	started = psutil.Process().create_time()
	completed = subprocess.run(command, env={
		**os.environ,
		SCOPED: '1',
		STARTED: repr(started),
		RECORD: record['id'],
	})
	if _recorded(args.results, record['id']):
		return completed.returncode
	record['status'] = 'failed'
	record['reason'] = (
		f"The run ended with status {completed.returncode} within a "
		f"memory limit of {memory_max}, without recording itself: the "
		"limit killed it, or systemd could not apply it."
	)
	return _finish(args.results, record)

def run(args: argparse.Namespace, parts: dict[str, dict[str, Any]]) -> int:
	"""Run one point, record it, and report it."""
	record = results.new_record(args.procedure)
	record['id'] = os.environ.get(RECORD) or record['id']
	try:
		settings = _environment(parts['environment'])
	except ValueError as error:
		record['status'] = 'failed'
		record['reason'] = str(error)
		return _finish(args.results, record)
	if settings['memory_max'] is not None and not os.environ.get(SCOPED):
		return _within_memory_limit(args, parts, str(settings['memory_max']))

	started = float(
		os.environ.get(STARTED) or psutil.Process().create_time()
	)
	try:
		procedure = procedures.get(args.procedure)
		record['environment'] = dict(settings)

		# Before vitrio starts a thread, so that every thread inherits it.
		if settings['cpus'] is not None:
			record['environment']['cpus'] = environment.set_cpus(
				str(settings['cpus'])
			)
		if hasattr(os, 'sched_getaffinity'):
			record['environment']['usable_cpus'] = len(os.sched_getaffinity(0))
		if settings['memory_max'] is not None:
			limit = environment.memory_limit()
			record['environment']['memory_limit_bytes'] = limit
			if limit is None:
				raise environment.Unavailable(
					"A memory limit was asked for, and none applies."
				)
		environment.raise_open_file_limit()

		spec = datasets.make_spec(parts['dataset'])
		dataset = datasets.load(args.datadir, spec)
		record['dataset'] = dataset.describe()
		scratchdir = settings['scratchdir']
		if hasattr(procedure, 'make_reader'):
			reader = procedure.make_reader(dataset, parts['reader'], scratchdir)
		else:
			reader_type = readers.get(
				parts['reader'].get('name', 'vitrio-loader')
			)
			reader = reader_type(dataset, parts['reader'], scratchdir)
		record['reader'] = reader.describe()
		workload = procedure.make_workload(parts['workload'], dataset)
		record['workload'] = workload

		target = environment.target(dataset.path)
		record['environment'].update({
			'machine': environment.save_machine(Path(args.results).parent),
			'target': target,
		})
		record['build'] = environment.build()

		reason = reader.unavailable()
		if reason:
			raise readers.Skip(reason)

		cached = [*dataset.stacks, *reader.files()]
		cache.evict(cached, settings['cold'])
		record['environment']['residency'] = cache.residency(cached)

		nfs = target.get('filesystem', '').startswith('nfs')
		nfs_before = nfs_read_bytes(target['mountpoint']) if nfs else None
		metrics = procedure.run(dataset, reader, workload, started)
		if nfs_before is not None:
			metrics['nfs_read_bytes'] = (
				nfs_read_bytes(target['mountpoint']) - nfs_before
			)
		if settings['memory_max'] is not None:
			metrics['cgroup_memory_peak_bytes'] = environment.memory_peak()

		record['reader'] = reader.describe()
		record['metrics'] = metrics
		record['status'] = 'ok'
	except readers.Skip as error:
		record['status'] = 'skipped'
		record['reason'] = str(error)
	except Exception as error:
		traceback.print_exc()
		record['status'] = 'failed'
		record['reason'] = f"{type(error).__name__}: {error}"
	return _finish(args.results, record)

def main(argv: list[str] | None = None) -> int:
	parser = argparse.ArgumentParser(
		prog='python -m vitrio_bench',
		description="The harness of vitrio's benchmarks."
	)
	commands = parser.add_subparsers(dest='command', required=True)
	commands.add_parser('env', help="describe this machine")
	generate = commands.add_parser(
		'generate', help="generate a dataset, unless it exists"
	)
	generate.add_argument('--datadir', required=True)
	cold = commands.add_parser(
		'cold', help="drop files from the page cache, and say how much is left"
	)
	cold.add_argument('mode', choices=cache.MODES)
	cold.add_argument('paths', nargs='+')
	real = commands.add_parser(
		'dataset', help="list the manifests of real datasets, or check one"
	)
	actions = real.add_subparsers(dest='action', required=True)
	actions.add_parser('list', help="list the manifests")
	checked = actions.add_parser(
		'check', help="check that a dataset downloaded is whole"
	)
	checked.add_argument('name', help="a manifest's name, or its path")
	checked.add_argument('root', help="the directory it was downloaded into")
	point = commands.add_parser('run', help="run one point, and record it")
	point.add_argument('procedure', choices=sorted(procedures.PROCEDURES))
	point.add_argument('--datadir', required=True)
	point.add_argument('--results', required=True)

	args, rest = parser.parse_known_args(argv)
	args.argv = list(sys.argv[1:] if argv is None else argv)
	parts = {'generate': ('dataset',), 'run': RUN_PARTS}.get(args.command, ())
	try:
		gathered = parse_parts(rest, parts)
	except ValueError as error:
		parser.error(str(error))

	if args.command == 'env':
		description = environment.machine()
		print(json.dumps(
			{'id': environment.machine_id(description), **description},
			indent='\t', sort_keys=True
		))
		return 0
	if args.command == 'generate':
		dataset = datasets.generate(
			args.datadir, datasets.make_spec(gathered['dataset'])
		)
		print(dataset.path)
		return 0
	if args.command == 'cold':
		cache.evict(args.paths, args.mode)
		print(f"{PREFIX} residency {cache.residency(args.paths)}")
		return 0
	if args.command == 'dataset':
		if args.action == 'list':
			for name in manifests.names():
				manifest = manifests.load(name)
				print(f"{manifest['name']}: {manifest['title']}")
			return 0
		problems = manifests.check(manifests.load(args.name), args.root)
		for problem in problems:
			print(problem)
		print(f"{PREFIX} dataset {'incomplete' if problems else 'whole'}")
		return 1 if problems else 0
	return run(args, gathered)

if __name__ == '__main__':
	sys.exit(main())
