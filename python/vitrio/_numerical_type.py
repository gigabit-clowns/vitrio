# SPDX-License-Identifier: LGPL-2.1-or-later

"""Teaches `NumericalType` the other ways a data type is stated.

`NumericalType(x)` is how Python turns a value into a member of an
enumeration. This module makes it take the name of a member and whatever
`numpy.dtype` takes, such as a type of numpy or the name numpy gives it. The
functions of `vitrio` pass their `data_type` argument through it.

The conversion is installed onto the bound enumeration, not onto a subclass
of it: the members the binding hands out are those of the bound one.
"""

from __future__ import annotations

from ._binding import NumericalType

# The types numpy names differently. It counts the bits of a complex number,
# where vitrio names the type of its two components.
_NUMPY_NAMES = {
	'bool': NumericalType.boolean,
	'complex64': NumericalType.complex_float32,
	'complex128': NumericalType.complex_float64,
}

def _find_by_name(cls, name: str) -> NumericalType | None:
	if name in _NUMPY_NAMES:
		return _NUMPY_NAMES[name]
	return cls.__members__.get(name)

# Called by the enumeration for a value that is not one of its members'.
# Returning None makes it raise the ValueError it raises for any other.
def _find_member(cls, value: object) -> NumericalType | None:
	if isinstance(value, str):
		member = _find_by_name(cls, value)
		if member is not None:
			return member

	# numpy reads None as its default type, which nobody asked for.
	if value is None:
		return None

	# numpy is not required. Without it only the names of the members are
	# understood.
	try:
		import numpy as np  # noqa: PLC0415
		name = np.dtype(value).name
	except (ImportError, TypeError):
		return None

	return _find_by_name(cls, name)

NumericalType._missing_ = classmethod(_find_member)
