# SPDX-License-Identifier: LGPL-2.1-or-later

"""The command line of the harness.

	python -m vitrio_bench env
	python -m vitrio_bench generate --datadir DIR [--dataset.NAME=VALUE ...]
	python -m vitrio_bench cold {none,fadvise,drop} PATH...
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
import sys
import traceback
from pathlib import Path
from typing import Any

import psutil

from . import cache, datasets, environment, procedures, readers, results
from .metrics import nfs_read_bytes

PREFIX = 'vitrio_bench'
RUN_PARTS = ('dataset', 'reader', 'workload', 'environment')
ENVIRONMENT = {'cold': 'none', 'cpus': None}

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

def run(args: argparse.Namespace, parts: dict[str, dict[str, Any]]) -> int:
	"""Run one point, record it, and report it."""
	started = psutil.Process().create_time()
	record = results.new_record(args.procedure)
	try:
		procedure = procedures.get(args.procedure)
		settings = _environment(parts['environment'])
		record['environment'] = dict(settings)

		# Before vitrio starts a thread, so that every thread inherits it.
		if settings['cpus'] is not None:
			record['environment']['cpus'] = environment.set_cpus(
				str(settings['cpus'])
			)
		if hasattr(os, 'sched_getaffinity'):
			record['environment']['usable_cpus'] = len(os.sched_getaffinity(0))
		environment.raise_open_file_limit()

		spec = datasets.make_spec(parts['dataset'])
		dataset = datasets.load(args.datadir, spec)
		record['dataset'] = dataset.describe()
		reader_type = readers.get(parts['reader'].get('name', 'vitrio-loader'))
		reader = reader_type(dataset, parts['reader'])
		record['reader'] = reader.describe()
		workload = procedure.make_workload(parts['workload'], dataset)
		record['workload'] = workload

		target = environment.target(dataset.path)
		record['environment'].update({
			'machine': environment.save_machine(Path(args.results).parent),
			'target': target,
		})
		record['build'] = environment.build()

		needed = readers.missing(reader_type)
		if needed:
			raise readers.Skip(f"{reader_type.name} needs {needed}.")

		cache.evict(dataset.stacks, settings['cold'])
		record['environment']['residency'] = cache.residency(dataset.stacks)

		nfs = target.get('filesystem', '').startswith('nfs')
		nfs_before = nfs_read_bytes(target['mountpoint']) if nfs else None
		metrics = procedure.run(dataset, reader, workload, started)
		if nfs_before is not None:
			metrics['nfs_read_bytes'] = (
				nfs_read_bytes(target['mountpoint']) - nfs_before
			)

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
	record['finished'] = results.now()
	results.append(args.results, record)
	_report(record)
	return 1 if record['status'] == 'failed' else 0

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
	point = commands.add_parser('run', help="run one point, and record it")
	point.add_argument('procedure', choices=sorted(procedures.PROCEDURES))
	point.add_argument('--datadir', required=True)
	point.add_argument('--results', required=True)

	args, rest = parser.parse_known_args(argv)
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
	return run(args, gathered)

if __name__ == '__main__':
	sys.exit(main())
