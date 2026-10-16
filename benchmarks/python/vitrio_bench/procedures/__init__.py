# SPDX-License-Identifier: LGPL-2.1-or-later

"""The procedures a run can follow: what it does with a reader, and what it
measures while it does.

A procedure is a module with:

- `make_workload(params, dataset)`: its workload, with the defaults of what
  is not given written out;
- `run(dataset, reader, workload, started)`: what it does, which returns
  its metrics. `started` is when the process began, in the seconds of
  `time.time`, from which the time to the first batch is counted;
- `make_reader(dataset, params, scratchdir)`, optionally: the reader of a
  procedure that does not read with those of `vitrio_bench.readers`, such
  as fio for the ceiling.
"""

from __future__ import annotations

from types import ModuleType

from . import ceiling, epochs, read_batches

PROCEDURES: dict[str, ModuleType] = {
	'ceiling': ceiling,
	'epochs': epochs,
	'read_batches': read_batches,
}

def get(name: str) -> ModuleType:
	"""
	The procedure of a name.

	Raises:
		ValueError: If no procedure has that name.
	"""
	try:
		return PROCEDURES[name]
	except KeyError:
		raise ValueError(
			f"There is no procedure {name!r}. There are {sorted(PROCEDURES)}."
		) from None
