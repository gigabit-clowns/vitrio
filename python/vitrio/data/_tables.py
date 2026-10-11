# SPDX-License-Identifier: LGPL-2.1-or-later

"""Tables of metadata as columns, and the locations of images in them.

A table is whatever the metadata of a dataset was parsed into: a pandas
DataFrame, a mapping of names to arrays, or a numpy structured array, which
is what `np.load` makes of a cryoSPARC `.cs` file. vitrio parses none of
the files themselves.
"""

from __future__ import annotations

import os
from collections.abc import Iterable, Mapping
from typing import Any

import numpy as np

from .._binding import ImageLocation

Key = str | os.PathLike

def as_columns(table: Any) -> dict[str, np.ndarray]:
	"""
	Split a table into one array for each of its columns.

	Args:
		table: A pandas DataFrame, a mapping of names to array-likes or a
			numpy structured array.

	Returns:
		dict[str, np.ndarray]: The columns by name, in the order of the
		table. Each holds a row of the table at each index of its first
		axis.

	Raises:
		TypeError: If `table` is none of the above.
		ValueError: If the columns are not all as long.
	"""
	if isinstance(table, np.ndarray):
		if table.dtype.names is None:
			raise TypeError(
				"An array is a table only when it is structured, with a "
				"field for each column."
			)
		columns = {name: table[name] for name in table.dtype.names}
	elif isinstance(table, Mapping):
		columns = {
			str(name): np.asarray(values) for name, values in table.items()
		}
	elif hasattr(table, 'columns') and hasattr(table, 'to_numpy'):
		columns = {
			str(name): table[name].to_numpy() for name in table.columns
		}
	else:
		raise TypeError(
			f"A {type(table).__name__} is not a table. A table is a "
			"DataFrame, a mapping of names to arrays or a structured array."
		)

	check_lengths(columns)
	return columns

def check_lengths(columns: Mapping[str, Any]) -> int | None:
	"""
	Check that columns are all as long, and say how long.

	Returns:
		The length of the columns, or None when there are none.

	Raises:
		ValueError: If two columns are not as long.
	"""
	length = None
	first = None
	for name, column in columns.items():
		if length is None:
			length, first = len(column), name
		elif len(column) != length:
			raise ValueError(
				f"The column {name!r} has {len(column)} rows, and "
				f"{first!r} has {length}."
			)
	return length

def _to_text(value: Any) -> str:
	if isinstance(value, bytes):
		return value.decode()
	return os.fspath(value) if isinstance(value, os.PathLike) else str(value)

def _resolve(key: str, root: Key | None) -> str:
	if root is None or os.path.isabs(key):
		return key
	return os.path.join(os.fspath(root), key)

def parse_locations(
	texts: Iterable[Any],
	root: Key | None = None
) -> list[ImageLocation]:
	"""
	Parse locations written as text, as RELION writes them.

	A location is written `"index@key"`, the index counting from one, or
	`"key"` for a whole file. That is how the `rlnImageName` and
	`rlnMicrographName` columns of a STAR file have them.

	Args:
		texts: The locations, as strings or bytes.
		root: The directory a relative key is relative to. Defaults to
			leaving it relative, which is to the working directory.

	Returns:
		list[ImageLocation]: The locations, in the order of `texts`.

	Raises:
		ValueError: If a text is not a location.
	"""
	locations = []
	for text in texts:
		location = ImageLocation.from_string(_to_text(text))
		key = _resolve(location.key, root)
		if key != location.key:
			location = ImageLocation(key, location.index_in_stack)
		locations.append(location)
	return locations

def make_locations(
	keys: Iterable[Any],
	indices: Iterable[int] | None = None,
	root: Key | None = None
) -> list[ImageLocation]:
	"""
	Make locations out of keys and the indices in their stacks.

	That is how cryoSPARC states where a particle is: its `blob/path` and
	`blob/idx` fields, the index counting from zero. The `>` that cryoSPARC
	writes in front of a path relative to its project is left out.

	Args:
		keys: The file of each location, as strings, bytes or paths.
		indices: The index of each location in its stack, counting from
			zero. Defaults to locations of whole files.
		root: The directory a relative key is relative to. Defaults to
			leaving it relative, which is to the working directory.

	Returns:
		list[ImageLocation]: The locations, in the order of `keys`.

	Raises:
		ValueError: If `keys` and `indices` are not as long.
	"""
	keys = [_resolve(_to_text(key).removeprefix('>'), root) for key in keys]
	if indices is None:
		return [ImageLocation(key) for key in keys]

	indices = [int(index) for index in indices]
	if len(indices) != len(keys):
		raise ValueError(
			f"There are {len(keys)} keys and {len(indices)} indices."
		)
	return [ImageLocation(key, index) for key, index in zip(keys, indices)]
