# SPDX-License-Identifier: LGPL-2.1-or-later

"""Where the values of an MRC stack are, for the baselines that read them
without vitrio or mrcfile."""

from __future__ import annotations

import struct
from pathlib import Path

import numpy as np

_MODES = {0: 'i1', 1: 'i2', 2: 'f4', 6: 'u2', 12: 'f2'}
HEADER_BYTES = 1024

def layout(path: str | Path) -> tuple[tuple[int, int, int], np.dtype, int]:
	"""
	The shape of a stack's values, their data type, and the offset of the
	first one in the file.

	Raises:
		ValueError: If the header states a mode this module does not read.
	"""
	with open(path, 'rb') as file:
		header = file.read(HEADER_BYTES)
	# MACHST: 0x44 0x44 for little endian, 0x11 0x11 for big.
	order = '>' if header[212] == 0x11 else '<'
	nx, ny, nz, mode = struct.unpack(order + '4i', header[:16])
	(extended,) = struct.unpack(order + 'i', header[92:96])
	if mode not in _MODES:
		raise ValueError(f"{path} stores mode {mode}, which is not read here.")
	dtype = np.dtype(order + _MODES[mode])
	return (nz, ny, nx), dtype, HEADER_BYTES + extended
