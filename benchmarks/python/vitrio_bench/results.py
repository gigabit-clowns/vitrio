# SPDX-License-Identifier: LGPL-2.1-or-later

"""The record a run leaves: one line of JSON per run.

A record holds what was run and what it measured, in five parts and the
metrics:

- `dataset`: what was read.
- `reader`: who read it, with every setting written out, defaults
  included.
- `workload`: how it was read.
- `environment`: where: the machine, the file system, the state of the
  page cache.
- `build`: which vitrio, and the versions of what ran beside it.

`schema` is bumped whenever a field changes meaning, so that whoever
analyses the records can tell them apart. That is the only contract with
the analysis, which does not live in this repository.
"""

from __future__ import annotations

import json
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
	import fcntl
except ImportError:
	fcntl = None

SCHEMA = 1
PARTS = ('dataset', 'reader', 'workload', 'environment', 'build')
STATUSES = ('ok', 'skipped', 'failed')

def now() -> str:
	"""The current time, as the records state it."""
	return datetime.now(timezone.utc).isoformat(timespec='milliseconds')

def new_record(procedure: str) -> dict[str, Any]:
	"""A record of a run that has just started, with nothing measured."""
	return {
		'schema': SCHEMA,
		'id': uuid.uuid4().hex,
		'procedure': procedure,
		'started': now(),
		'finished': None,
		'status': None,
		'reason': None,
		**{part: {} for part in PARTS},
		'metrics': {},
	}

def check(record: dict[str, Any]) -> None:
	"""
	Check that a record follows the schema.

	Raises:
		ValueError: If a field is missing or holds what it must not.
	"""
	expected = {
		'schema', 'id', 'procedure', 'started', 'finished', 'status',
		'reason', 'metrics', *PARTS,
	}
	if set(record) != expected:
		raise ValueError(
			f"A record holds {sorted(expected)}, not {sorted(record)}."
		)
	if record['schema'] != SCHEMA:
		raise ValueError(f"Schema {record['schema']} is not {SCHEMA}.")
	if record['status'] not in STATUSES:
		raise ValueError(f"Status {record['status']!r} is none of {STATUSES}.")
	if record['status'] != 'ok' and not record['reason']:
		raise ValueError("A run that did not succeed says why.")
	for part in (*PARTS, 'metrics'):
		if not isinstance(record[part], dict):
			raise ValueError(f"The part {part!r} is not a mapping.")

def _plain(value: Any) -> Any:
	# numpy scalars, and paths, as JSON takes them.
	if hasattr(value, 'item'):
		return value.item()
	if isinstance(value, Path):
		return str(value)
	raise TypeError(f"{type(value).__name__} is not written to a record.")

def dumps(record: dict[str, Any]) -> str:
	"""A record as one line of JSON."""
	return json.dumps(record, sort_keys=True, default=_plain)

def append(path: str | Path, record: dict[str, Any]) -> None:
	"""
	Append a record to a file of records.

	The file is locked while the line is written, so runs that end at the
	same time do not interleave their lines.
	"""
	path = Path(path)
	path.parent.mkdir(parents=True, exist_ok=True)
	line = dumps(record) + '\n'
	with open(path, 'a', encoding='utf-8') as file:
		if fcntl is not None:
			fcntl.flock(file, fcntl.LOCK_EX)
		file.write(line)
		file.flush()

def load(path: str | Path) -> list[dict[str, Any]]:
	"""Every record of a file of records, in the order they were written."""
	with open(path, encoding='utf-8') as file:
		return [json.loads(line) for line in file if line.strip()]
